// Haptics — beeps + vibration for the Core2.
//
// Speaker beeps use M5.Speaker.tone() (NS4168 I2S), which M5.begin() initialises.
// The vibration motor is driven via M5.Power.setVibration(level) (AXP192 LDO3),
// 0 = stop. vibrate() starts a non-blocking pulse; update() stops it on expiry.
#pragma once

#include <stdint.h>

class Haptics {
public:
    void begin() {}              // speaker already up after M5.begin()
    void setEnabled(bool on) { _enabled = on; }

    void keyTick();   // 800 Hz, 10 ms
    void click();     // 1000 Hz, 20 ms
    void confirm();   // 880 Hz, 60 ms
    void cancel();    // 400 Hz, 80 ms
    void poke();      // 1200 Hz, 40 ms
    void sendTone();  // 660 Hz, 50 ms

    // Vibration motor (AXP192 LDO3) via M5.Power.setVibration(level), 0 = stop.
    void vibrate(uint8_t ms = 60); // start a non-blocking pulse
    void update(uint32_t nowMs);   // stop the pulse when it expires (call each frame)

private:
    void _tone(uint32_t freq, uint32_t ms);

    bool _enabled = true;
    uint32_t _vibrateUntil = 0;
};
