#include <Arduino.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "KeyMacro.h"
#include "TextMacro.h"
#include "EEPROMUtils.h"

#define ARR_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define KEY_DELAY 5

using SequenceAction = KeyMacro::SequenceAction;
using SequenceActionType = KeyMacro::SequenceActionType;

const uint8_t BUTTONS[] = {2, 3, 4, 5, 6, 7, 8, 9, 10};
const unsigned long DEBOUNCE_TIME = 50; // 50ms

unsigned long debounceMillis = 0;
bool prevState[ARR_SIZE(BUTTONS)];

constexpr size_t MACRO_COUNT = 3;
constexpr size_t LARGEST_EEPROM_OBJ_SIZE = max(sizeof(TextMacro), sizeof(KeyMacro));
uint8_t macroTypeFlag;
bool isMacroFlagEmpty = false;
constexpr size_t FLAG_INDEX = 0;
constexpr size_t FLAG_SIZE = sizeof(macroTypeFlag);
ProgrammableMacro *macros[MACRO_COUNT];

void loadMacros();
void toggleMacroFlagBit(uint8_t &, size_t);
uint8_t readMacroFlagBit(uint8_t, size_t);
template <typename T>
bool isIndexEmpty(int);
size_t macroIndexToEEPROMIndex(size_t idx);
void handleCommands(const String &);
void handleReadCommand();
void handleWriteCommand(JsonDocument &);
void handleClearCommand(size_t idx);

void setup()
{
	Serial.begin(115200);

	while (!Serial)
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
		Serial.println(buf);

		handleCommands(buf);
	}

	if ((millis() - debounceMillis) < DEBOUNCE_TIME)
		return;

	for (size_t i = 0; i < ARR_SIZE(BUTTONS); i++)
	{
		bool currentState = digitalRead(BUTTONS[i]);

		if (prevState[i] == HIGH && currentState == LOW)
		{
			switch (i)
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

			case 3:
				BootKeyboard.press(KEY_LEFT_CTRL);
				BootKeyboard.press(KEY_TILDE);
				delay(100);
				BootKeyboard.releaseAll();
				BootKeyboard.print("npm run dev");
				delay(100);
				BootKeyboard.press(KEY_ENTER);
				break;

			case 4:
				BootKeyboard.press(KEY_LEFT_CTRL);
				BootKeyboard.press('s');
				break;

			case 5:
				if (macros[0] != nullptr)
					macros[0]->execute();
				break;

			case 6:
				if (macros[1] != nullptr)
					macros[1]->execute();
				break;

			case 7:
				if (macros[2] != nullptr)
					macros[2]->execute();
				break;

			default:
				Serial.println(F("Unregistered macro"));
				break;
			}

			delay(KEY_DELAY);
			Consumer.releaseAll();
			BootKeyboard.releaseAll();
		}

		prevState[i] = currentState;
	}

	debounceMillis = millis();
}

void loadMacros()
{
	/*
		basically checking if the first
		byte is empty where the flag is stored
	*/
	isMacroFlagEmpty = isIndexEmpty<uint8_t>(FLAG_INDEX);
	if (!isMacroFlagEmpty)
	{
		macroTypeFlag = EEPROM.read(FLAG_INDEX);

		Serial.print(F("Flag index in EEPROM exists: "));
		Serial.println(macroTypeFlag, BIN);
		Serial.println(F("Loading macros..."));

		for (size_t i = 0; i < MACRO_COUNT; i++)
		{
			size_t objIdx = macroIndexToEEPROMIndex(i);

			if (isIndexEmpty<TextMacro>(objIdx))
				continue;

			if (readMacroFlagBit(macroTypeFlag, i) == (uint8_t)MacroType::KEY)
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

		Serial.println(F("Macros loaded successfully"));
	}
	else
	{
		Serial.println(F("Flag index in EEPROM is empty."));
		Serial.println(F("Initializing empty macros array..."));
		for (size_t i = 0; i < MACRO_COUNT; i++)
		{
			macros[i] = nullptr;
		}
	}
}

void toggleMacroFlagBit(uint8_t &flag, size_t bit)
{
	flag ^= 1 << bit;
}

uint8_t readMacroFlagBit(uint8_t flag, size_t bit)
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
			x: Reset macros
			r: Read macros
			w: Write macros

			Allows us to easily add more
			functionality in the future
		*/
		switch (ev[0])
		{
		case 'x':
			Serial.println(F("Resetting EEPROM..."));
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

			Serial.println(F("EEPROM reset successfully"));
			break;

		case 'r':
			handleReadCommand();
			break;

		case 'w':
			handleWriteCommand(doc);
			break;

			/*
			case 'c':
			{
				size_t macroIndex = doc["macroIndex"].as<size_t>();
				handleClearCommand(macroIndex);
				break;
			}
			*/

		default:
			Serial.println(F("Invalid event"));
			break;
		}
	}
}

void handleReadCommand()
{
	if (isMacroFlagEmpty)
	{
		// construct an array with null
		// elements based on the macro count
		String ret = "[";
		for (size_t i = 0; i < MACRO_COUNT; i++)
		{
			ret += "null";

			if (i != MACRO_COUNT - 1)
			{
				ret += ", ";
			}
		}
		ret += "]";
		Serial.println(ret);

		return;
	}

	JsonDocument readDoc;
	JsonArray dataArr = readDoc.to<JsonArray>();
	for (size_t i = 0; i < MACRO_COUNT; i++)
	{
		JsonObject obj = dataArr.add<JsonObject>();
		if (macros[i] == nullptr)
			continue;

		ProgrammableMacro *macro = macros[i];
		if (macro->getType() == MacroType::KEY)
		{
			KeyMacro *keyMacro = (KeyMacro *)macro;
			obj["macroType"] = (uint8_t)macro->getType();

			JsonArray seqArr = obj["macroData"].to<JsonArray>();
			JsonObject seqObj;
			SequenceAction *seq = keyMacro->getSequence();
			for (size_t i = 0; i < keyMacro->getSeqLen(); i++)
			{
				SequenceAction action = seq[i];
				seqObj = seqArr.add<JsonObject>();
				seqObj["sequenceType"] = (uint8_t)action.type;

				switch (action.type)
				{
				case SequenceActionType::RELEASE_ALL:
					break;
				
				case SequenceActionType::KEYSTROKE: [[fallthrough]]
				case SequenceActionType::CONSUMER_KEYSTROKE:
					seqObj["keycode"] = (KeyboardKeycode)action.keycode;
					break;

				case SequenceActionType::DELAY:
					seqObj["delay"] = action.delay;
					break;
				}
			}
		}
		else if (macro->getType() == MacroType::TEXT)
		{
			TextMacro *textMacro = (TextMacro *)macro;

			obj["macroType"] = (uint8_t)textMacro->getType();
			obj["macroData"] = textMacro->getText();
		}
	}

	serializeJson(readDoc, Serial);
}

void handleWriteCommand(JsonDocument &doc)
{
	MacroType macroType = (MacroType)doc["macroType"].as<uint8_t>();
	size_t macroIndex = doc["macroIndex"].as<size_t>();

	if (macroIndex > (MACRO_COUNT - 1))
	{
		Serial.println(F("Invalid macro index"));
		return;
	}

	if (macroType == MacroType::KEY)
	{
		JsonArray macroSeq = doc["macroData"].as<JsonArray>();
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
				.type = (SequenceActionType)obj["sequenceType"].as<uint8_t>()};

			switch (act.type)
			{
			case SequenceActionType::RELEASE_ALL:
				break;

			case SequenceActionType::KEYSTROKE: [[fallthrough]]
			case SequenceActionType::CONSUMER_KEYSTROKE:
				act.keycode = obj["keycode"].as<uint16_t>();
				break;

			case SequenceActionType::DELAY:
				act.delay = obj["delay"].as<uint16_t>();
				break;
			}

			keyMacro->addSeqAction(act);
		}

		if (readMacroFlagBit(macroTypeFlag, macroIndex) != (uint8_t)MacroType::KEY)
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
		String macroText = doc["macroData"].as<String>();
		TextMacro *textMacro = (TextMacro *)macros[macroIndex];

		if (textMacro)
		{
			delete textMacro;
		}

		size_t bufLen = TextMacro::TEXT_LEN - 1;
		char bufArr[bufLen];
		macroText.toCharArray(bufArr, bufLen);
		textMacro = new TextMacro(bufArr);

		if (readMacroFlagBit(macroTypeFlag, macroIndex) != (uint8_t)MacroType::TEXT)
		{
			toggleMacroFlagBit(macroTypeFlag, macroIndex);
		}

		size_t macroEEPROMIdx = macroIndexToEEPROMIndex(macroIndex);

		EEPROMUtils::reset<TextMacro>(macroEEPROMIdx);

		EEPROM.put(FLAG_INDEX, macroTypeFlag);
		EEPROM.put(macroEEPROMIdx, *textMacro);

		macros[macroIndex] = textMacro;
	}

	Serial.println(F("Macro added successfully"));
}

void handleClearCommand(size_t idx)
{
	if (idx > (MACRO_COUNT - 1))
	{
		Serial.println(F("Invalid macro index"));
		return;
	}

	size_t eepromIdx = macroIndexToEEPROMIndex(idx);
	EEPROMUtils::reset<TextMacro>(eepromIdx);

	Serial.print(F("Cleared macro in index "));
	Serial.println(idx);
}