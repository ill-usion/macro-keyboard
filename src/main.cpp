#include <Arduino.h>
#include <EEPROM.h>
#include <assert.h>
#include <avr/wdt.h>
#include <CRC32.h>
#include "Macro.h"
#include "EEPROMUtils.h"
#include "SerialUtils.h"

// #define KEYBOARD_DEBUG
#ifdef KEYBOARD_DEBUG
#define DEBUG_PRINT Serial.print
#define DEBUG_PRINTLN Serial.println
#else
#define DEBUG_PRINT
#define DEBUG_PRINTLN
#endif

#define ARR_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define KEY_DELAY 5

// RGB led pin config
#define RED_PIN 3
#define GREEN_PIN 5
#define BLUE_PIN 6

typedef struct
{
    uint8_t r, g, b;
} RGB;

enum class MacroKeyboardOperation : uint8_t
{
    READ = 0x00,
    WRITE = 0x01,
    COMMAND = 0x02
};
typedef MacroKeyboardOperation KbdOp;

enum class MacroKeyboardCommand : uint8_t
{
    RESET = 0x00,         // No arguments. No return.
    SWITCH_LAYERS = 0x01, // No arguments. Returns the current layer after switching.
    PRESS_MACRO = 0x02    // Takes macro index as a byte (uint8_t). No return.
};
typedef MacroKeyboardCommand KbdCmd;

enum class MacroKeyboardReturnCode : uint8_t
{
    OK,
    OUT_OF_RANGE
};
typedef MacroKeyboardReturnCode KbdRet;

const uint8_t NUM_ROWS = 3;
const uint8_t NUM_COLUMNS = 4;
const uint8_t LAYER_SWITCH_BTN_IDX = 2;
const uint8_t BUTTONS[] = {2, A0, 4, A1, A2, 7, 8, 9, 10, 16, 14, 15};
// Button at index 2 is used to switch between layers
const bool PROGRAMMABLE_BUTTONS[] = {true, true, false, true, true, true, true, true, true, true, true, true};
static_assert(ARR_SIZE(BUTTONS) == ARR_SIZE(PROGRAMMABLE_BUTTONS), "Lenght of `BUTTONS` and `PROGRAMMABLE_BUTTONS` do not match.");

const unsigned long DEBOUNCE_TIME = 10; // 10ms
unsigned long debounceMillis = 0;
bool prevState[ARR_SIZE(BUTTONS)];

constexpr size_t LAYER_COUNT = 2;
constexpr size_t MACRO_COUNT = 11; // Number of programmable macros per layer
// Precompute the total EEPROM usage
constexpr size_t TOTAL_MACRO_COUNT = MACRO_COUNT * LAYER_COUNT;
constexpr size_t EEPROM_MACRO_SIZE = NUM_ACTIONS_PER_KEY * sizeof(uint16_t);
constexpr size_t TOTAL_MACRO_EEPROM_SIZE = TOTAL_MACRO_COUNT * EEPROM_MACRO_SIZE;
constexpr size_t LAYER_SIZE = TOTAL_MACRO_EEPROM_SIZE / LAYER_COUNT;
constexpr size_t COLORS_SIZE = LAYER_COUNT * sizeof(RGB);

// Saved layer index comes right after macro data
#define CURRENT_LAYER_EEPROM_IDX TOTAL_MACRO_EEPROM_SIZE
/*
EEPROM Layout:
    1012 bytes (Macro data)
    1 byte (Current later)
    6 bytes (Layer colors)

Total: 1019 bytes
    5 bytes free
*/
constexpr size_t TOTAL_EEPROM_USAGE = TOTAL_MACRO_EEPROM_SIZE + COLORS_SIZE + sizeof(uint8_t);
static_assert(!(TOTAL_EEPROM_USAGE > 1024), "Insufficient EEPROM memory.");

constexpr unsigned long WAIT_FOR_SERIAL_TIME = 3000; // wait 3s for serial to open

// Follows the index approach when denoting a layer
// layer 1 -> 0
// layer 2 -> 1
uint8_t currentLayer;
Macro macros[MACRO_COUNT];
RGB layerColors[LAYER_COUNT];

// Forward declerations
void loadConfig();
void loadMacros();
void loadLedColors();
void cycleLayers();
void processOperation(KbdOp op);
void handleReadOp();
void handleWriteOp();
void handleCommand();
void resetKeyboard();
void handleSwitchLayerCmd();
void setLedColor(const RGB &color);
void transitionToColor(const RGB &start, const RGB &end, int steps, int delayMs);
template <typename T>
T clamp(const T &n, const T &min, const T &max);

void setup()
{
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);

    // Initially turn off the LED
    setLedColor({0, 0, 0});
    // digitalWrite(RED_PIN, HIGH);
    // digitalWrite(GREEN_PIN, HIGH);
    // digitalWrite(BLUE_PIN, HIGH);

    Serial.begin(9600);

    unsigned long waitForSerialStartTime = millis();
    // stops waiting for serial to open after the defined amount of time
    while (!Serial && (millis() - waitForSerialStartTime) < WAIT_FOR_SERIAL_TIME)
        ;

    loadConfig();
    loadLedColors();
    setLedColor(layerColors[currentLayer]);

    for (size_t i = 0; i < ARR_SIZE(BUTTONS); i++)
    {
        // Configure all buttons as input pullup
        // to eliminate the use of pullup resistors
        pinMode(BUTTONS[i], INPUT_PULLUP);
        prevState[i] = HIGH;
    }

    Consumer.begin();
    Consumer.releaseAll();

    BootKeyboard.begin();
    BootKeyboard.releaseAll();
}

void loop()
{
    if (Serial.available())
    {
        uint8_t byte;
        Serial.readBytes(&byte, sizeof(uint8_t));

        KbdOp op = (KbdOp)byte;
        processOperation(op);
    }

    if ((millis() - debounceMillis) < DEBOUNCE_TIME)
        return;

    for (size_t btnIdx = 0; btnIdx < ARR_SIZE(BUTTONS); btnIdx++)
    {
        bool currentState = digitalRead(BUTTONS[btnIdx]);

        if (prevState[btnIdx] == HIGH && currentState == LOW)
        {
            if (btnIdx == LAYER_SWITCH_BTN_IDX)
            {

                DEBUG_PRINTLN(F("Switching layers"));
                cycleLayers();
                goto UPDATE_STATE;
            }

            if (!PROGRAMMABLE_BUTTONS[btnIdx])
            {
                DEBUG_PRINTLN(F("Unprogrammable button"));
                goto UPDATE_STATE;
            }

            size_t idxInSeq = 1;
            for (size_t i = 0; i < ARR_SIZE(PROGRAMMABLE_BUTTONS); i++)
            {
                if (i == btnIdx)
                    break;

                if (!PROGRAMMABLE_BUTTONS[i])
                    goto UPDATE_STATE;

                idxInSeq++;
            }
            //                 2. flip             1. calc the idx
            size_t macroIdx = (MACRO_COUNT - 1) - (MACRO_COUNT - idxInSeq);
            macros[macroIdx].execute();

            delay(KEY_DELAY);
            Consumer.releaseAll();
            BootKeyboard.releaseAll();
        }
    UPDATE_STATE:
        prevState[btnIdx] = currentState;
    }

    debounceMillis = millis();
}

void loadConfig()
{
    size_t currLayerIdx = CURRENT_LAYER_EEPROM_IDX;
    currentLayer = clamp(EEPROM.read(currLayerIdx), (uint8_t)0, (uint8_t)(LAYER_COUNT - 1));

    loadMacros();
}

void loadMacros()
{
    Action actions[NUM_ACTIONS_PER_KEY];
    size_t macroIdx = 0;

    // For each macro
    for (size_t macro = 0; macro < MACRO_COUNT; macro++)
    {
        size_t actionIdx = 0;

        // For each action slot in the macro
        for (size_t i = 0; i < NUM_ACTIONS_PER_KEY; i++)
        {
            size_t eepromIdx = (macro * NUM_ACTIONS_PER_KEY + i) * 2 + (LAYER_SIZE * currentLayer);

            uint16_t packed = EEPROM.read(eepromIdx) | (EEPROM.read(eepromIdx + 1) << 8);

            DEBUG_PRINT(F("EEPROM IDX: "));
            DEBUG_PRINT(eepromIdx);
            DEBUG_PRINT(F(" PACKED: "));
            DEBUG_PRINTLN(packed, HEX);

            if (packed != 0)
            {
                actions[actionIdx++] = Action(packed);
            }
        }

        DEBUG_PRINT("LOADING MACRO ");
        DEBUG_PRINT(macroIdx);
        DEBUG_PRINT(F(" with "));
        DEBUG_PRINT(actionIdx);
        DEBUG_PRINTLN(F(" actions"));

        macros[macroIdx++] = Macro(actions, actionIdx);
    }
}

void loadLedColors()
{
    // After layer index
    size_t colorsStartIdx = CURRENT_LAYER_EEPROM_IDX + 1;

    // 1 color per layer
    for (size_t i = 0; i < LAYER_COUNT; i++)
    {
        EEPROM.get(colorsStartIdx + (i * sizeof(RGB)), layerColors[i]);
    }
}

void cycleLayers()
{
    const RGB &prevColor = layerColors[currentLayer];
    currentLayer = (currentLayer + 1) % LAYER_COUNT;
    const RGB &currColor = layerColors[currentLayer];

    EEPROM.write(CURRENT_LAYER_EEPROM_IDX, currentLayer);
    loadMacros();

    transitionToColor(prevColor, currColor, 50, 5);
}

void processOperation(KbdOp op)
{
    switch (op)
    {
    case KbdOp::READ:
        handleReadOp();
        break;

    case KbdOp::WRITE:
        handleWriteOp();
        break;

    case KbdOp::COMMAND:
        handleCommand();
        break;

    default:
        break;
    }
}

void handleReadOp()
{
    uint16_t idx = read_uint16();
    uint16_t length = read_uint16();

    if (idx + length > EEPROM.length())
    {
        Serial.write((uint8_t)KbdRet::OUT_OF_RANGE);
        while (Serial.available())
            Serial.read();
        return;
    }

    uint8_t *payload = new uint8_t[length];

    for (uint16_t i = 0; i < length; i++)
    {
        payload[i] = EEPROM.read(idx + i);
    }

    uint32_t checksum = CRC32::calculate(payload, length);

    Serial.write((uint8_t)KbdRet::OK);
    Serial.write((uint8_t *)&checksum, sizeof(uint32_t));
    Serial.write((uint8_t *)&length, sizeof(uint16_t));
    Serial.write(payload, length);

    delete[] payload;
}

void handleWriteOp()
{
    uint16_t idx = read_uint16();
    uint16_t length = read_uint16();

    if (idx + length > EEPROM.length())
    {
        Serial.write((uint8_t)KbdRet::OUT_OF_RANGE);
        while (Serial.available())
            Serial.read();

        return;
    }

    uint8_t *data = new uint8_t[length];
    Serial.readBytes(data, length);

    uint32_t checksum = CRC32::calculate(data, length);

    for (uint16_t i = 0; i < length; i++)
    {
        EEPROM.write(idx + i, data[i]);
    }

    delete[] data;

    Serial.write((uint8_t)KbdRet::OK);
    Serial.write((uint8_t *)&checksum, sizeof(uint32_t));
}

void handleCommand()
{
    uint8_t byte;
    Serial.readBytes(&byte, sizeof(uint8_t));

    KbdCmd cmd = (KbdCmd)byte;
    switch (cmd)
    {
    case KbdCmd::RESET:
        resetKeyboard();
        break;

    case KbdCmd::SWITCH_LAYERS:
        handleSwitchLayerCmd();
        break;

    case KbdCmd::PRESS_MACRO:
        // TODO
        break;

    default:
        break;
    }
}

void resetKeyboard()
{
    wdt_enable(WDTO_15MS);
    while (1)
        ;
}

void handleSwitchLayerCmd()
{
    cycleLayers();
    Serial.write((uint8_t)KbdRet::OK);
    Serial.write(currentLayer);
}

void setLedColor(const RGB &color)
{
    // Edit this according to your LED type
    analogWrite(RED_PIN, 255 - color.r);
    analogWrite(GREEN_PIN, 255 - color.g);
    analogWrite(BLUE_PIN, 255 - color.b);
}

void transitionToColor(const RGB &start, const RGB &end, int steps, int delayMs)
{
    for (int i = 0; i <= steps; i++)
    {
        RGB current;
        current.r = start.r + (end.r - start.r) * i / steps;
        current.g = start.g + (end.g - start.g) * i / steps;
        current.b = start.b + (end.b - start.b) * i / steps;

        setLedColor(current);
        delay(delayMs);
    }
}

template <typename T>
T clamp(const T &n, const T &min, const T &max)
{
    return n <= min ? min : n >= max ? max
                                     : n;
}