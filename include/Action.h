#pragma once
#include <stdint.h>

enum class ActionType : uint8_t
{
    KEYSTROKE = 0,
    CONSUMER_KEYSTROKE = 1,
    DELAY = 2,
    RELEASE_ALL = 3
};

enum class KeyState : uint8_t
{
    RELEASE = 0,
    PRESS = 1,
    PRESS_AND_RELEASE = 2
};

class Action
{
private:
    ActionType m_type = ActionType::KEYSTROKE;
    KeyState m_state = KeyState::RELEASE;
    uint16_t m_data = 0;

public:
    // Empty action
    Action() = default;

    // Decode from packed 16-bit value
    Action(uint16_t packed);

    // Create Action from components
    Action(ActionType type, KeyState state, uint16_t data);

    // Encode into packed 16-bit value
    uint16_t pack() const;

    ActionType type() const { return m_type; }
    KeyState state() const { return m_state; }
    uint16_t data() const { return m_data; }
};
