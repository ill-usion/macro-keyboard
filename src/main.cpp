#include <Arduino.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <assert.h>
#include "KeyMacro.h"
#include "TextMacro.h"
#include "EEPROMUtils.h"

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

using SequenceAction = KeyMacro::SequenceAction;
using SequenceActionType = KeyMacro::SequenceActionType;

const uint8_t NUM_ROWS = 3;
const uint8_t NUM_COLUMNS = 4;
const uint8_t BUTTONS[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 14, 15};
const bool PROGRAMMABLE_BUTTONS[] = {false, false, false, true, true, true, true, true, true, true, true, true};
const unsigned long DEBOUNCE_TIME = 10; // 10ms

unsigned long debounceMillis = 0;
bool prevState[ARR_SIZE(BUTTONS)];

// 16 bits data type allows for up to 15 custom macros
// which is more than enough for a 3x4 macro keyboard
uint16_t macroTypeFlag;
bool isMacroFlagEmpty = false;
constexpr size_t FLAG_INDEX = 0;
constexpr size_t FLAG_SIZE = sizeof(macroTypeFlag);

constexpr size_t MACRO_COUNT = 9; // number of programmable macros
constexpr size_t LARGEST_EEPROM_OBJ_SIZE = max(sizeof(TextMacro), sizeof(KeyMacro));
constexpr size_t USED_EEPROM_SIZE = (MACRO_COUNT * LARGEST_EEPROM_OBJ_SIZE);

// reserve 1 byte for the macro flag
static_assert(!(USED_EEPROM_SIZE - FLAG_SIZE > 1024), "Insufficient EEPROM memory. Try decreasing MACRO_COUNT.");

ProgrammableMacro *macros[MACRO_COUNT];
constexpr unsigned long WAIT_FOR_SERIAL_TIME = 3000; // wait 3s for serial to open

void loadMacros();
void toggleMacroFlagBit(uint16_t &, size_t);
uint16_t readMacroFlagBit(uint16_t, size_t);
template <typename T>
bool isIndexEmpty(int);
size_t macroIndexToEEPROMIndex(size_t idx);
void handleCommands(const String &);
void handleIdentifyCommand();
void handleReadAllCommand();
void handleReadCommand(size_t);
void handleWriteCommand(JsonDocument &);
void handleClearCommand(size_t idx);

void setup()
{
	Serial.begin(115200);

	unsigned long waitForSerialStartTime = millis();
	// stops waiting for serial to open after the defined amount of time
	while (!Serial && (millis() - waitForSerialStartTime) < WAIT_FOR_SERIAL_TIME)
		;

	loadMacros();

	for (size_t i = 0; i < ARR_SIZE(BUTTONS); i++)
	{
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
		String buf = Serial.readStringUntil('\n');
		DEBUG_PRINTLN(buf);

		handleCommands(buf);
	}

	if ((millis() - debounceMillis) < DEBOUNCE_TIME)
		return;

	for (size_t btnIdx = 0; btnIdx < ARR_SIZE(BUTTONS); btnIdx++)
	{
		bool currentState = digitalRead(BUTTONS[btnIdx]);

		if (prevState[btnIdx] == HIGH && currentState == LOW)
		{
			switch (btnIdx)
			{
			case 0:
				BootKeyboard.press(KEY_LEFT_CTRL);
				BootKeyboard.press('c');
				break;

			case 1:
				BootKeyboard.press(KEY_LEFT_CTRL);
				BootKeyboard.press('v');
				break;

			case 2:
				BootKeyboard.press(KEY_LEFT_ALT);
				BootKeyboard.press(KEY_TAB);
				break;

			default:
			{
				if (!PROGRAMMABLE_BUTTONS[btnIdx])
				{
					DEBUG_PRINTLN(F("Unregistered macro"));
					break;
				}

				size_t idxInSeq = 1;
				for (size_t i = 0; i < ARR_SIZE(PROGRAMMABLE_BUTTONS); i++)
				{
					if (i == btnIdx)
						break;

					if (!PROGRAMMABLE_BUTTONS[i])
						continue;

					idxInSeq++;
				}
				//                 2. flip             1. calc the idx
				size_t macroIdx = (MACRO_COUNT - 1) - (MACRO_COUNT - idxInSeq);

				if (macros[macroIdx] == nullptr)
				{
					DEBUG_PRINTLN(F("Unassigned macro"));
					break;
				}

				macros[macroIdx]->execute();
				break;
			}
			}

			delay(KEY_DELAY);
			Consumer.releaseAll();
			BootKeyboard.releaseAll();
		}

		prevState[btnIdx] = currentState;
	}

	debounceMillis = millis();
}

void loadMacros()
{
	// basically checking if the first
	// byte is empty where the flag is stored
	isMacroFlagEmpty = isIndexEmpty<uint16_t>(FLAG_INDEX);
	if (!isMacroFlagEmpty)
	{
		macroTypeFlag = EEPROM.read(FLAG_INDEX);

		DEBUG_PRINT(F("Flag index in EEPROM exists: "));
		DEBUG_PRINTLN(macroTypeFlag, BIN);
		DEBUG_PRINTLN(F("Loading macros..."));

		for (size_t i = 0; i < MACRO_COUNT; i++)
		{
			size_t objIdx = macroIndexToEEPROMIndex(i);

			if (isIndexEmpty<TextMacro>(objIdx))
				continue;

			if (readMacroFlagBit(macroTypeFlag, i) == (uint16_t)MacroType::KEY)
			{
				KeyMacro *keyMacro = new KeyMacro();
				EEPROM.get(objIdx, *keyMacro);
				macros[i] = keyMacro;
			}
			else
			{
				TextMacro *textMacro = new TextMacro();
				EEPROM.get(objIdx, *textMacro);
				macros[i] = textMacro;
			}
		}

		DEBUG_PRINTLN(F("Macros loaded successfully"));
	}
	else
	{
		DEBUG_PRINTLN(F("Flag index in EEPROM is empty."));
		DEBUG_PRINTLN(F("Initializing empty macros array..."));
		for (size_t i = 0; i < MACRO_COUNT; i++)
		{
			macros[i] = nullptr;
		}
	}
}

void toggleMacroFlagBit(uint16_t &flag, size_t bit)
{
	flag ^= 1 << bit;
}

uint16_t readMacroFlagBit(uint16_t flag, size_t bit)
{
	return (flag >> bit) & 1;
}

template <typename T>
bool isIndexEmpty(int idx)
{
	size_t len = sizeof(T);
	for (size_t i = idx; i < (idx + len); i += 1)
	{
		// Factory value for EEPROM is 255 (0xFF)
		if (EEPROM.read(i) != 0xFF)
			return false;
	}

	return true;
}

size_t macroIndexToEEPROMIndex(size_t idx)
{
	return (LARGEST_EEPROM_OBJ_SIZE * idx) + FLAG_SIZE;
}

void handleCommands(const String &buffer)
{
	// For debug purposes
	if (buffer.startsWith("DUMP"))
	{
		EEPROMUtils::dump();
	}
	else
	{
		JsonDocument doc;
		DeserializationError error = deserializeJson(doc, buffer);
		if (error)
		{
			Serial.print(F("Error deserializing buffer: "));
			Serial.println(error.f_str());

			return;
		}

		// the library doesn't allow us to convert to char for some reason
		const char *ev = doc["event"].as<const char *>();
		if (strlen(ev) == 0)
		{
			Serial.println(F("Expected string of length > 0 for field `event`"));
			return;
		}

		/*
			i: Identify
			x: Reset macros
			r: Read macros
			a: Read all macros
			w: Write macros
			c: Reset a specific button
		*/
		switch (ev[0])
		{
		case 'i':
			handleIdentifyCommand();
			break;

		case 'x':
			DEBUG_PRINTLN(F("Resetting EEPROM..."));
			EEPROMUtils::reset();
			for (size_t i = 0; i < MACRO_COUNT; i++)
			{
				ProgrammableMacro *ptr = macros[i];
				if (ptr)
				{
					delete ptr;
					macros[i] = nullptr;
				}
			}

			DEBUG_PRINTLN(F("EEPROM reset successfully"));
			break;

		case 'r':
		{
			size_t macroIndex = doc["index"].as<size_t>();
			handleReadCommand(macroIndex);
			break;
		}

		case 'a':
		{
			handleReadAllCommand();
			break;
		}

		case 'w':
			handleWriteCommand(doc);
			break;

		case 'c':
		{
			size_t macroIndex = doc["index"].as<size_t>();
			handleClearCommand(macroIndex);
			break;
		}

		default:
			Serial.println(F("Invalid event"));
			break;
		}
	}
}

void handleIdentifyCommand()
{
	JsonDocument doc;
	doc["rows"] = NUM_ROWS;
	doc["cols"] = NUM_COLUMNS;

	doc["nPins"] = ARR_SIZE(BUTTONS);
	JsonArray pinsArr = doc["pins"].to<JsonArray>();
	for (size_t i = 0; i < ARR_SIZE(BUTTONS); i++)
	{
		pinsArr.add(BUTTONS[i]);
	}

	doc["nProgPins"] = ARR_SIZE(PROGRAMMABLE_BUTTONS);
	JsonArray progPinsArr = doc["progPins"].to<JsonArray>();
	for (size_t i = 0; i < ARR_SIZE(PROGRAMMABLE_BUTTONS); i++)
	{
		if (PROGRAMMABLE_BUTTONS[i])
			progPinsArr.add(BUTTONS[i]);
	}

	serializeJson(doc, Serial);
	Serial.println();
}

void handleReadAllCommand()
{
	if (isMacroFlagEmpty)
	{
		// construct an array with `null`
		// elements based on the macro count
		Serial.print('[');
		for (size_t i = 0; i < MACRO_COUNT; i++)
		{
			Serial.print("null");

			if (i != MACRO_COUNT - 1)
			{
				Serial.print(',');
			}
		}
		Serial.println(']');

		return;
	}

	Serial.print('[');
	JsonDocument readDoc;
	for (size_t macroIdx = 0; macroIdx < MACRO_COUNT; macroIdx++)
	{
		ProgrammableMacro *macro = macros[macroIdx];
		if (macro == nullptr)
		{
			Serial.print(F("null"));
			continue;
		}
		else
		{

			readDoc.clear();
			readDoc["index"] = macroIdx;
			switch (macro->getType())
			{
			case MacroType::KEY:
			{
				KeyMacro *keyMacro = (KeyMacro *)macro;
				readDoc["type"] = (uint8_t)macro->getType();

				JsonArray seqArr = readDoc["data"].to<JsonArray>();
				JsonObject seqObj;
				SequenceAction *seq = keyMacro->getSequence();
				for (size_t i = 0; i < keyMacro->getSeqLen(); i++)
				{
					SequenceAction action = seq[i];
					seqObj = seqArr.add<JsonObject>();
					seqObj["sType"] = (uint8_t)action.type;

					switch (action.type)
					{
					case SequenceActionType::RELEASE_ALL:
						break;

					case SequenceActionType::KEYSTROKE:
					[[fallthrough]]
					case SequenceActionType::CONSUMER_KEYSTROKE:
					[[fallthrough]]
					case SequenceActionType::CHARACTER_KEYSTROKE:
						seqObj["keycode"] = (uint16_t)action.keycode;
						break;

					case SequenceActionType::DELAY:
						seqObj["delay"] = action.delay;
						break;
					}
				}
				break;
			}

			case MacroType::TEXT:
			{
				TextMacro *textMacro = (TextMacro *)macro;

				readDoc["type"] = (uint8_t)textMacro->getType();
				readDoc["data"] = textMacro->getText();
				break;
			}
			}

			serializeJson(readDoc, Serial);
		}

		if (macroIdx < MACRO_COUNT - 1)
		{
			Serial.print(",");
		}
	}

	Serial.println("]");
}

void handleReadCommand(size_t macroIdx)
{
	if (macroIdx > (MACRO_COUNT - 1))
	{
		Serial.println(F("Invalid macro index"));
		return;
	}

	ProgrammableMacro *macro = macros[macroIdx];
	if (macro == nullptr)
	{
		Serial.println(F("null"));
		return;
	}

	JsonDocument readDoc;
	readDoc["index"] = macroIdx;
	switch (macro->getType())
	{
	case MacroType::KEY:
	{
		KeyMacro *keyMacro = (KeyMacro *)macro;
		readDoc["type"] = (uint8_t)macro->getType();

		JsonArray seqArr = readDoc["data"].to<JsonArray>();
		JsonObject seqObj;
		SequenceAction *seq = keyMacro->getSequence();
		for (size_t i = 0; i < keyMacro->getSeqLen(); i++)
		{
			SequenceAction action = seq[i];
			seqObj = seqArr.add<JsonObject>();
			seqObj["sType"] = (uint8_t)action.type;

			switch (action.type)
			{
			case SequenceActionType::RELEASE_ALL:
				break;

			case SequenceActionType::KEYSTROKE:
			[[fallthrough]]
			case SequenceActionType::CONSUMER_KEYSTROKE:
			[[fallthrough]]
			case SequenceActionType::CHARACTER_KEYSTROKE:
				seqObj["keycode"] = (uint16_t)action.keycode;
				break;

			case SequenceActionType::DELAY:
				seqObj["delay"] = action.delay;
				break;
			}
		}
		break;
	}

	case MacroType::TEXT:
	{
		TextMacro *textMacro = (TextMacro *)macro;

		readDoc["type"] = (uint8_t)textMacro->getType();
		readDoc["data"] = textMacro->getText();
		break;
	}
	}

	serializeJson(readDoc, Serial);
	Serial.println();
}

void handleWriteCommand(JsonDocument &doc)
{
	MacroType macroType = (MacroType)doc["type"].as<uint8_t>();
	size_t macroIndex = doc["index"].as<size_t>();

	if (macroIndex > (MACRO_COUNT - 1))
	{
		Serial.println(F("Invalid macro index"));
		return;
	}

	if (macroType == MacroType::KEY)
	{
		JsonArray macroSeq = doc["data"].as<JsonArray>();
		KeyMacro *keyMacro = (KeyMacro *)macros[macroIndex];

		if (keyMacro)
		{
			delete keyMacro;
		}

		keyMacro = new KeyMacro();
		SequenceAction act;
		for (auto it = macroSeq.begin(); it != macroSeq.end(); ++it)
		{
			auto obj = *it;
			act = {
				.type = (SequenceActionType)obj["sType"].as<uint8_t>()};

			switch (act.type)
			{
			case SequenceActionType::RELEASE_ALL:
				break;

			case SequenceActionType::KEYSTROKE:
			[[fallthrough]]
			case SequenceActionType::CONSUMER_KEYSTROKE:
			[[fallthrough]]
			case SequenceActionType::CHARACTER_KEYSTROKE:
				act.keycode = obj["keycode"].as<uint16_t>();
				break;

			case SequenceActionType::DELAY:
				act.delay = obj["delay"].as<uint16_t>();
				break;
			}

			keyMacro->addSeqAction(act);
		}

		if (readMacroFlagBit(macroTypeFlag, macroIndex) != (uint16_t)MacroType::KEY)
		{
			toggleMacroFlagBit(macroTypeFlag, macroIndex);
		}

		size_t macroEEPROMIdx = macroIndexToEEPROMIndex(macroIndex);

		EEPROMUtils::reset<TextMacro>(macroEEPROMIdx);

		EEPROM.put(FLAG_INDEX, macroTypeFlag);
		EEPROM.put(macroEEPROMIdx, *keyMacro);

		macros[macroIndex] = keyMacro;
	}
	else
	{
		String macroText = doc["data"].as<String>();
		TextMacro *textMacro = (TextMacro *)macros[macroIndex];

		if (textMacro)
		{
			delete textMacro;
		}

		size_t bufLen = TextMacro::TEXT_LEN;
		char bufArr[bufLen];
		macroText.toCharArray(bufArr, bufLen);
		textMacro = new TextMacro(bufArr);

		if (readMacroFlagBit(macroTypeFlag, macroIndex) != (uint16_t)MacroType::TEXT)
		{
			toggleMacroFlagBit(macroTypeFlag, macroIndex);
		}

		size_t macroEEPROMIdx = macroIndexToEEPROMIndex(macroIndex);

		EEPROMUtils::reset<TextMacro>(macroEEPROMIdx);

		EEPROM.put(FLAG_INDEX, macroTypeFlag);
		EEPROM.put(macroEEPROMIdx, *textMacro);

		macros[macroIndex] = textMacro;
	}

	handleReadCommand(macroIndex);
}

void handleClearCommand(size_t idx)
{
	if (idx > (MACRO_COUNT - 1))
	{
		Serial.println(F("Invalid macro"));
		return;
	}

	if (macros[idx] == nullptr)
	{
		Serial.print(F("Macro "));
		Serial.print(idx + 1);
		Serial.println(F(" is unset"));
		return;
	}

	size_t eepromIdx = macroIndexToEEPROMIndex(idx);
	EEPROMUtils::reset<TextMacro>(eepromIdx);

	delete macros[idx];
	macros[idx] = nullptr;

	Serial.print(F("Reset macro "));
	Serial.print(idx + 1);
	Serial.println(F(" successfully"));
}
