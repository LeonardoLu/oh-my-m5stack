#pragma once

#include <stdint.h>

#include "LightingState.h"

class BottomLeds {
public:
    bool begin();
    void setBrightness(uint8_t level);
    void update(const LightingState& lighting, uint32_t nowMs, bool reducedMotion,
                bool connected);
    bool available() const { return _available; }

private:
    bool _available = false;
    uint8_t _brightness = 2;
    uint32_t _lastFrameMs = 0;
};
