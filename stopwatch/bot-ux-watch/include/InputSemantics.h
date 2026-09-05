#pragma once

#include <stdint.h>

namespace watchinput {

enum class Gesture : uint8_t { None, Tap, Long, SwipeUp, SwipeDown };

class ButtonGesture {
public:
    Gesture poll(bool wasPressed, bool isPressed, bool wasReleased, uint32_t nowMs) {
        if (wasPressed) {
            _downMs = nowMs;
            _longSent = false;
        }
        if (isPressed && !_longSent && nowMs - _downMs >= kLongMs) {
            _longSent = true;
            return Gesture::Long;
        }
        if (wasReleased && !_longSent) return Gesture::Tap;
        return Gesture::None;
    }

    void consume(bool isPressed) { _longSent = isPressed; }

private:
    static constexpr uint32_t kLongMs = 650;
    uint32_t _downMs = 0;
    bool _longSent = false;
};

class TouchGesture {
public:
    Gesture poll(bool wasPressed, bool isPressed, bool wasReleased,
                 int16_t x, int16_t y, uint32_t nowMs) {
        if (wasPressed) {
            _down = true;
            _moved = _longSent = _swipeSent = false;
            _x0 = _x = x;
            _y0 = _y = y;
            _downMs = nowMs;
            return Gesture::None;
        }
        if (isPressed && _down) {
            _x = x;
            _y = y;
            int16_t dx = _x - _x0;
            int16_t dy = _y - _y0;
            if (magnitude(dx) > 24 || magnitude(dy) > 24) _moved = true;
            if (_moved && !_longSent && !_swipeSent
                && magnitude(dy) > 52 && magnitude(dy) > magnitude(dx)) {
                _swipeSent = true;
                return dy < 0 ? Gesture::SwipeUp : Gesture::SwipeDown;
            }
            if (!_moved && !_longSent && nowMs - _downMs >= kLongMs) {
                _longSent = true;
                return Gesture::Long;
            }
        }
        if (wasReleased && _down) {
            _down = false;
            if (!_moved && !_longSent && !_swipeSent) {
                _tapX = _x;
                _tapY = _y;
                return Gesture::Tap;
            }
        }
        return Gesture::None;
    }

    void consume() {
        _down = false;
        _moved = false;
        _longSent = false;
        _swipeSent = false;
    }

    int16_t tapX() const { return _tapX; }
    int16_t tapY() const { return _tapY; }

private:
    static constexpr uint32_t kLongMs = 650;

    static int32_t magnitude(int16_t value) {
        return value < 0 ? -(int32_t)value : value;
    }

    bool _down = false;
    bool _moved = false;
    bool _longSent = false;
    bool _swipeSent = false;
    int16_t _x0 = 0, _y0 = 0, _x = 0, _y = 0;
    int16_t _tapX = 0, _tapY = 0;
    uint32_t _downMs = 0;
};

} // namespace watchinput
