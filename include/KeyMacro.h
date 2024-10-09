#pragma once
#include "ProgrammableMacro.h"

class KeyMacro : public ProgrammableMacro
{
public:
    enum class SequenceActionType : uint8_t
    {
        RELEASE_ALL = 0x00,
        KEYSTROKE = 0x01,
        DELAY = 0x02
    };

    typedef struct _SequenceAction
    {
        SequenceActionType type;
        union
        {
            uint16_t delay;
            uint8_t keycode;
        };
    } SequenceAction;

    KeyMacro() = default;
    KeyMacro(SequenceAction* sequence, size_t len);

    void execute() override;
    MacroType getType() const override;
    size_t getSeqLen() const;
    SequenceAction* getSequence() const;
    void addSeqAction(SequenceAction action);

public:
    static const size_t NUM_SEQUENCE = 16;

private:
    size_t m_seqLen = 0;
    SequenceAction m_actionSequence[NUM_SEQUENCE];
};
