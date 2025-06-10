#include "Macro.h"

Macro::Macro(const Action *actions, size_t len)
{
    size_t copy_len = (len > NUM_ACTIONS_PER_KEY) ? NUM_ACTIONS_PER_KEY : len;
    for (size_t i = 0; i < copy_len; ++i)
        m_actions[i] = actions[i];
    m_len = copy_len;
}

void Macro::execute()
{
    uint16_t data;
    for (size_t i = 0; i < m_len; i++)
    {
        const Action &action = m_actions[i];
        data = action.data();
        switch (action.type())
        {
        case ActionType::KEYSTROKE:
        case ActionType::CONSUMER_KEYSTROKE:
            executeKeystroke(i);
            break;

        case ActionType::DELAY:
            delay(data);
            break;

        case ActionType::RELEASE_ALL:
            BootKeyboard.releaseAll();
            Consumer.releaseAll();
            break;

        default:
            // Do nothing
            break;
        }
        delay(ACTION_DELAY);
    }
}

const Action &Macro::getAction(size_t idx) const
{
    if (idx < m_len)
    {
        return m_actions[idx];
    }

    static Action defaultAction;
    return defaultAction;
}

void Macro::executeKeystroke(size_t idx)
{
    if (idx > m_len - 1)
        return;

    const Action &action = m_actions[idx];

    uint16_t actionData = action.data();
    switch (action.state())
    {
    case KeyState::RELEASE:
        if (action.type() == ActionType::KEYSTROKE)
            BootKeyboard.release((KeyboardKeycode)actionData);
        else if (action.type() == ActionType::CONSUMER_KEYSTROKE)
            Consumer.release((ConsumerKeycode)actionData);

        break;

    case KeyState::PRESS:
        if (action.type() == ActionType::KEYSTROKE)
            BootKeyboard.press((KeyboardKeycode)actionData);
        else if (action.type() == ActionType::CONSUMER_KEYSTROKE)
            Consumer.press((ConsumerKeycode)actionData);

        break;

    case KeyState::PRESS_AND_RELEASE:
        if (action.type() == ActionType::KEYSTROKE)
        {
            BootKeyboard.press((KeyboardKeycode)actionData);
            delay(PRESS_AND_RELEASE_DELAY);
            BootKeyboard.release((KeyboardKeycode)actionData);
        }
        else if (action.type() == ActionType::CONSUMER_KEYSTROKE)
        {
            Consumer.press((ConsumerKeycode)actionData);
            delay(PRESS_AND_RELEASE_DELAY);
            Consumer.release((ConsumerKeycode)actionData);
        }
        break;

    default:
        break;
    }
}