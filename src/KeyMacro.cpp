#include "KeyMacro.h"

KeyMacro::KeyMacro(SequenceAction* sequence, size_t len)
{
    m_seqLen = min(len, NUM_SEQUENCE);

    for (size_t i = 0; i < m_seqLen; ++i)
    {
        m_actionSequence[i] = sequence[i];
    }
}

MacroType KeyMacro::getType() const
{
    return MacroType::KEY;
}

void KeyMacro::execute()
{
    for (size_t i = 0; i < m_seqLen; ++i)
    {
        SequenceAction& seq = m_actionSequence[i];
        switch (seq.type)
        {
        case SequenceActionType::DELAY:
            delay(seq.delay);
            break;

        case SequenceActionType::KEYSTROKE:
            BootKeyboard.press((KeyboardKeycode)seq.keycode);
            break;

        case SequenceActionType::CONSUMER_KEYSTROKE:
            Consumer.press((ConsumerKeycode)seq.keycode);
            break;

        case SequenceActionType::CHARACTER_KEYSTROKE:
            BootKeyboard.press((char)seq.keycode);
            break;

        case SequenceActionType::RELEASE_ALL:
            BootKeyboard.releaseAll();
            Consumer.releaseAll();
            break;
        }
    }
}

size_t KeyMacro::getSeqLen() const
{
    return m_seqLen;
}

KeyMacro::SequenceAction* KeyMacro::getSequence() const
{
    return (KeyMacro::SequenceAction*)m_actionSequence;
}

void KeyMacro::addSeqAction(SequenceAction action)
{
    if (m_seqLen < NUM_SEQUENCE)
    {
        m_actionSequence[m_seqLen++] = action;
    }
}
