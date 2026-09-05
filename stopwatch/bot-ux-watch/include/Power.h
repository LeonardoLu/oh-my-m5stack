// StopWatch power facade. M5Unified owns the detected M5PM1 instance.
#pragma once

#include <stdint.h>

class Power {
public:
    void begin();

    void update();
    uint8_t batteryPct() const { return _batteryPct; }
    bool charging() const { return _charging; }

    void applyLevel(uint8_t level); // 1..5 -> 0..255 -> display
    void setBrightness(uint8_t v);  // raw 0..255

    static uint8_t levelToValue(uint8_t level);

private:
    uint16_t _filteredMv = 0;
    uint8_t _batteryPct = 100;
    bool _charging = false;
};
