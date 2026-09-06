#pragma once

#include <stdint.h>

#include "LightingState.h"
#include "BottomLedFrame.h"

class BottomLeds {
public:
    bool begin();
    void setBrightness(uint8_t level);
    void setMode(uint8_t mode); // 0 Off, 1 exact host renderer, 2 Alive (default)
    void interact(uint8_t selected, uint32_t nowMs);
    void notify(uint8_t mask, uint32_t nowMs);
    // Local control feedback is rendered only in Alive mode. Existing host
    // zones retain their hue; color fills otherwise-unused ambient pixels.
    void control(uint32_t color, uint32_t nowMs);
    void hold(uint32_t color, bool active, uint32_t nowMs);
    void update(const LightingState& lighting, uint32_t nowMs, bool reducedMotion,
                bool connected, uint8_t selectedAgent = 0,
                uint8_t freshReplyMask = 0);
    bool available() const { return _available; }

private:
    bool _available = false;
    uint8_t _brightness = 2;
    uint8_t _mode = 2;
    bottomled::Events _events{};
    uint32_t _lastFrameMs = 0;
};
