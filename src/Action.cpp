#include "Action.h"

Action::Action(uint16_t packed)
{
    m_type = static_cast<ActionType>((packed >> 14) & 0b11);

    if (m_type == ActionType::KEYSTROKE || m_type == ActionType::CONSUMER_KEYSTROKE)
    {
        m_state = static_cast<KeyState>((packed >> 12) & 0b11);
        m_data = packed & 0x0FFF; // 12 bits
    }
    else
    {
        m_state = KeyState::RELEASE; // ignored
        m_data = packed & 0x3FFF;   // 14 bits (bits 13–0)
    }
}

Action::Action(ActionType type, KeyState state, uint16_t data)
{
    m_type = type;

    if (type == ActionType::KEYSTROKE || type == ActionType::CONSUMER_KEYSTROKE)
    {
        m_state = state;
        m_data = data & 0x0FFF; // 12-bit max
    }
    else
    {
        m_state = KeyState::RELEASE; // unused
        m_data = data & 0x3FFF;     // 14-bit max
    }
}

uint16_t Action::pack() const
{
    uint16_t result = (static_cast<uint16_t>(m_type) << 14);

    if (m_type == ActionType::KEYSTROKE || m_type == ActionType::CONSUMER_KEYSTROKE)
    {
        result |= (static_cast<uint16_t>(m_state) << 12);
        result |= (m_data & 0x0FFF); // 12 bits
    }
    else
    {
        result |= (m_data & 0x3FFF); // 14 bits
    }

    return result;
}
