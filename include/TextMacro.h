#pragma once

#include "ProgrammableMacro.h"

class TextMacro : public ProgrammableMacro
{
public:
    static const size_t TEXT_LEN = 100;
    
private:
    char m_text[TEXT_LEN];

public:
    TextMacro() = default;
    TextMacro(const char[TEXT_LEN]);
    void execute() override;
    const char *getText() const;
    MacroType getType() const override;
};