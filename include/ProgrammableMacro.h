#pragma once
#include <Arduino.h>
#include <HID-Project.h>

enum class MacroType : uint8_t
{
	KEY,
	TEXT
};

class ProgrammableMacro
{
public:
	virtual void execute() = 0;
	virtual MacroType getType() const = 0;
	virtual ~ProgrammableMacro() {}
};