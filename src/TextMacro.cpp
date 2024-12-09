#include "TextMacro.h"

TextMacro::TextMacro(const char text[TEXT_LEN])
{
    size_t len = min(strlen(text), sizeof(m_text));
    memcpy(m_text, text, len);
    m_text[len] = '\0';
}

void TextMacro::execute()
{
    BootKeyboard.print(m_text);
}

MacroType TextMacro::getType() const
{
    return MacroType::TEXT;
}

const char *TextMacro::getText() const
{
    return (const char *)m_text;
}
