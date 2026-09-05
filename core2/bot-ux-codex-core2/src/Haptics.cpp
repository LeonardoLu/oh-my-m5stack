// Haptics implementation. See Haptics.h for the contract.
#include "Haptics.h"

#include <M5Unified.h> // M5.Speaker, M5.Power
#include <Arduino.h>   // millis()

void Haptics::_tone(uint32_t freq, uint32_t ms)
{
    if (!_enabled) return;
    M5.Speaker.tone((float)freq, ms);
}

void Haptics::vibrate(uint8_t ms)
{
    if (!_enabled) return;
    M5.Power.setVibration(255);      // Core2 AXP192 LDO3 motor
    _vibrateUntil = millis() + ms;
}

void Haptics::update(uint32_t nowMs)
{
    if (_vibrateUntil && (int32_t)(nowMs - _vibrateUntil) >= 0) {
        _vibrateUntil = 0;
        M5.Power.setVibration(0);
    }
}

void Haptics::keyTick()  { _tone(800, 10); }
void Haptics::click()    { _tone(1000, 20); }
void Haptics::confirm()  { _tone(880, 60); }
void Haptics::cancel()   { _tone(400, 80); }
void Haptics::poke()     { _tone(1200, 40); }
void Haptics::sendTone() { _tone(660, 50); }
