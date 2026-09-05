// Power — battery, charging state, and display brightness on the Plus2.
#pragma once

#include <stdint.h>

class Power {
public:
    void begin();

    uint8_t batteryPct();           // 0..100
    bool    charging();

    void applyLevel(uint8_t level); // 1..5 -> 0..255 -> display
    void setBrightness(uint8_t v);  // raw 0..255

    static uint8_t levelToValue(uint8_t level);
};
