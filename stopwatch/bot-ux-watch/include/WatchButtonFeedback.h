#pragma once

#include <math.h>
#include <stdint.h>

namespace watchbuttons {

enum Button : uint8_t {
    A = 1 << 0,
    B = 1 << 1,
    Power = 1 << 2,
};

struct Blob {
    int16_t centerDegrees;
    int16_t halfDegrees;
    int16_t depth;
    uint16_t color;
};

// Angles use screen coordinates: 0 degrees points right and 90 degrees down.
// The locations and colors match the StopWatch enclosure: yellow A at the
// upper-left edge, blue B at the upper-right edge, and red power at the bottom.
constexpr Blob restingBlob(Button button) {
    return button == A ? Blob{220, 18, 24, 0xFFE0}
         : button == B ? Blob{320, 18, 24, 0x34BF}
                       : Blob{90, 15, 20, 0xF800};
}

inline Blob expandedBlob(Button button, uint8_t step) {
    static const uint8_t sideHalf[] = {2, 5, 9, 13, 17, 20, 19, 18, 18};
    static const uint8_t sideDepth[] = {8, 12, 17, 21, 24, 27, 25, 24, 24};
    static const uint8_t powerHalf[] = {2, 4, 7, 10, 14, 17, 16, 15, 15};
    static const uint8_t powerDepth[] = {7, 10, 14, 17, 20, 23, 21, 20, 20};
    if (step > 8) step = 8;
    Blob result = restingBlob(button);
    bool power = button == Power;
    result.halfDegrees = power ? powerHalf[step] : sideHalf[step];
    result.depth = power ? powerDepth[step] : sideDepth[step];
    return result;
}

inline float blobInset(const Blob& blob, float degrees) {
    float u = (degrees - blob.centerDegrees) / blob.halfDegrees;
    if (u <= -1.0f || u >= 1.0f) return 0.0f;
    float base = 1.0f - u * u;
    return blob.depth * base * sqrtf(base);
}

class Feedback {
public:
    static constexpr uint8_t ExpandSteps = 8;
    static constexpr uint32_t ExpandMs = 160;

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
