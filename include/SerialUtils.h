#pragma once
#include <Arduino.h>

uint8_t read_uint8()
{
    while (!Serial.available())
        ;
    return Serial.read();
}

uint16_t read_uint16()
{
    uint16_t val = read_uint8();
    return val | (read_uint8() << 8);
}
