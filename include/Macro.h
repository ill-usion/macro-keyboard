#pragma once
#include "Action.h"
#include <Arduino.h>
#include <HID-Project.h>

#define PRESS_AND_RELEASE_DELAY 50
#define ACTION_DELAY 25
#define NUM_ACTIONS_PER_KEY 23
class Macro
{
private:
    size_t m_len;
    Action m_actions[NUM_ACTIONS_PER_KEY];

public:
    // Default constructor
    Macro() = default;
    // Construct macro based on an array of actions
    Macro(const Action *actions, size_t len);

    // Execute macro actions
    void execute();

    // Get action at a specific index
    const Action &getAction(size_t idx) const;

    size_t length() const { return m_len; }

private:
    void executeKeystroke(size_t idx);
};