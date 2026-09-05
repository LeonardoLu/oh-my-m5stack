// Round-safe StopWatch face: RTC date/time, honest power status and bot hero.
#pragma once

#include <M5Unified.h>
#include <BotUx.h>
#include <stdint.h>

class WatchFace {
public:
    void begin(M5Canvas* cv, M5Canvas* botSprite);

    void setHour24(bool v)   { _hour24 = v; }
    void setShowSeconds(bool v) { _showSeconds = v; }
    void setBattery(uint8_t pct);
    void setCharging(bool v) { _charging = v; }

    void update(uint32_t nowMs);
    void draw(uint16_t ink, uint16_t muted, uint16_t accent,
              uint16_t panel, uint16_t warning);

    botux::BotUx& bot() { return _bot; }

    uint8_t hour()   const { return _hh; }
    uint8_t minute() const { return _mm; }
    uint8_t second() const { return _ss; }
    int16_t year() const { return _year; }
    uint8_t month() const { return _month; }
    uint8_t day() const { return _day; }

private:
    void _drawDate(uint16_t muted);
    void _drawBattery(uint16_t ink, uint16_t accent, uint16_t warning);
    void _drawClock(uint16_t ink, uint16_t muted, uint16_t accent);
    void _drawSetPill(uint16_t ink, uint16_t accent, uint16_t panel);
    static void _drawBolt(M5Canvas* cv, int16_t x, int16_t y, uint16_t c);

    M5Canvas* _cv = nullptr;
    botux::BotUx _bot;

    uint32_t _now = 0;
    uint8_t  _hh = 0, _mm = 0, _ss = 0;
    int16_t  _year = 2000;
    uint8_t  _month = 1, _day = 1, _weekDay = 0;
    bool     _hour24 = true;
    bool     _showSeconds = false;
    uint8_t  _battery = 100;
    bool     _charging = false;
};
