#pragma once

#include <stdint.h>

namespace watchbuttons {

enum Button : uint8_t {
    A = 1 << 0,
    B = 1 << 1,
    Power = 1 << 2,
};

struct Arc {
    int16_t startDegrees;
    int16_t endDegrees;
    uint16_t color;
};

// Angles use screen coordinates: 0 degrees points right and 90 degrees down.
// The locations and colors match the StopWatch enclosure: yellow A at the
// upper-left edge, blue B at the upper-right edge, and red power at the bottom.
constexpr Arc arc(Button button) {
    return button == A ? Arc{205, 235, 0xFFE0}
         : button == B ? Arc{305, 335, 0x34BF}
                       : Arc{78, 102, 0xF800};
}

class Feedback {
public:
    static constexpr uint8_t ExpandSteps = 6;
    static constexpr uint32_t ExpandMs = 120;

    bool sample(bool aPressed, bool bPressed, bool powerValid, bool powerPressed,
                uint32_t nowMs) {
        uint8_t next = (aPressed ? A : 0) | (bPressed ? B : 0)
            | (powerValid && powerPressed ? Power : 0);
        bool changed = next != _held;
        uint8_t pressed = next & ~_held;
        if (pressed & A) _pressedAt[0] = nowMs;
        if (pressed & B) _pressedAt[1] = nowMs;
        if (pressed & Power) _pressedAt[2] = nowMs;
        _held = next;
        if (changed) updateSteps(nowMs);
        return changed;
    }

    bool advance(uint32_t nowMs) {
        uint8_t prior[3] = {_steps[0], _steps[1], _steps[2]};
        updateSteps(nowMs);
        return prior[0] != _steps[0] || prior[1] != _steps[1] || prior[2] != _steps[2];
    }

    uint8_t held() const { return _held; }
    bool isHeld(Button button) const { return (_held & button) != 0; }
    uint8_t expandStep(Button button) const { return _steps[index(button)]; }

private:
    static uint8_t index(Button button) { return button == A ? 0 : button == B ? 1 : 2; }

    void updateSteps(uint32_t nowMs) {
        const Button buttons[3] = {A, B, Power};
        for (uint8_t i = 0; i < 3; ++i) {
            if (!isHeld(buttons[i])) {
                _steps[i] = 0;
                continue;
            }
            uint32_t elapsed = nowMs - _pressedAt[i];
            uint32_t step = elapsed * ExpandSteps / ExpandMs;
            _steps[i] = step > ExpandSteps ? ExpandSteps : (uint8_t)step;
        }
    }

    uint8_t _held = 0;
    uint8_t _steps[3] = {0, 0, 0};
    uint32_t _pressedAt[3] = {0, 0, 0};
};

} // namespace watchbuttons
