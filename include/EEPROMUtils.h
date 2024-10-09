#pragma once

#include <Arduino.h>
#include <EEPROM.h>

namespace EEPROMUtils
{
    static void reset()
    {
        for (size_t i = 0; i < E2END + 1; i++)
        {
            EEPROM.write(i, 0xFF);
        }
    }

    template <typename T>
    static void reset(size_t startIdx)
    {
        for (size_t i = startIdx; i < sizeof(T); i++)
        {
            EEPROM.write(i, 0xFF);
        }
    }

    static void dump(Stream *stream = &Serial)
    {
        for (size_t i = 0; i < E2END + 1; i++)
        {
            stream->print(EEPROM.read(i));
            stream->print(' ');
        }
        stream->println();
    }

    template <typename T>
    static void dump(size_t idx = 0, Stream *stream = &Serial)
    {
        for (size_t i = idx; i < sizeof(T); i++)
        {
            stream->print(EEPROM.read(i));
            stream->print(' ');
        }
        stream->println();
    }

} // namespace EEPROMUtils
