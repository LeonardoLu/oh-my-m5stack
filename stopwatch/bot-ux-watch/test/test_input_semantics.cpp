#include "InputSemantics.h"

#include <assert.h>

using watchinput::ButtonGesture;
using watchinput::Gesture;
using watchinput::TouchGesture;

static void testButtonTapAndLongPress() {
    ButtonGesture button;
    assert(button.poll(true, true, false, 100) == Gesture::None);
    assert(button.poll(false, false, true, 200) == Gesture::Tap);

    assert(button.poll(true, true, false, 1000) == Gesture::None);
    assert(button.poll(false, true, false, 1649) == Gesture::None);
    assert(button.poll(false, true, false, 1650) == Gesture::Long);
    assert(button.poll(false, true, false, 1900) == Gesture::None);
    assert(button.poll(false, false, true, 2000) == Gesture::None);
}

static void testConsumedButtonWake() {
    ButtonGesture button;
    assert(button.poll(true, true, false, 100) == Gesture::None);
    button.consume(true);
    assert(button.poll(false, true, false, 900) == Gesture::None);
    assert(button.poll(false, false, true, 910) == Gesture::None);
    assert(button.poll(true, true, false, 1000) == Gesture::None);
    assert(button.poll(false, false, true, 1010) == Gesture::Tap);
}

static void testTouchTap() {
    TouchGesture touch;
    assert(touch.poll(true, true, false, 120, 180, 100) == Gesture::None);
    assert(touch.poll(false, true, false, 123, 181, 150) == Gesture::None);
    assert(touch.poll(false, false, true, 123, 181, 180) == Gesture::Tap);
    assert(touch.tapX() == 123 && touch.tapY() == 181);
}

static void testTouchLongDoesNotRepeatOrTap() {
    TouchGesture touch;
    assert(touch.poll(true, true, false, 200, 200, 100) == Gesture::None);
    assert(touch.poll(false, true, false, 200, 200, 750) == Gesture::Long);
    assert(touch.poll(false, true, false, 200, 130, 900) == Gesture::None);
    assert(touch.poll(false, false, true, 200, 130, 920) == Gesture::None);
}

static void testSwipeDoesNotRepeatBecomeLongOrTap() {
    TouchGesture touch;
    assert(touch.poll(true, true, false, 200, 240, 100) == Gesture::None);
    assert(touch.poll(false, true, false, 202, 180, 180) == Gesture::SwipeUp);
    assert(touch.poll(false, true, false, 203, 130, 900) == Gesture::None);
    assert(touch.poll(false, false, true, 203, 130, 920) == Gesture::None);
}

static void testConsumedTouchWake() {
    TouchGesture touch;
    assert(touch.poll(true, true, false, 50, 50, 100) == Gesture::None);
    touch.consume();
    assert(touch.poll(false, true, false, 50, 50, 900) == Gesture::None);
    assert(touch.poll(false, false, true, 50, 50, 910) == Gesture::None);
    assert(touch.poll(true, true, false, 60, 70, 1000) == Gesture::None);
    assert(touch.poll(false, false, true, 60, 70, 1010) == Gesture::Tap);
}

int main() {
    testButtonTapAndLongPress();
    testConsumedButtonWake();
    testTouchTap();
    testTouchLongDoesNotRepeatOrTap();
    testSwipeDoesNotRepeatBecomeLongOrTap();
    testConsumedTouchWake();
    return 0;
}
