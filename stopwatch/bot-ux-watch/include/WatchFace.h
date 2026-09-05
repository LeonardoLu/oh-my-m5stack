// WatchFace — owns the bot-ux bot and renders the watch's own large clock plus
// the top status row (signal bars, battery % + charging bolt). The bot's own
// tiny time/label overlays are deliberately unused: the clock is the hero.
#pragma once

#include <M5Unified.h>
#include <BotUx.h>
#include <stdint.h>

class WatchFace {
public:
    void begin(M5Canvas* cv, M5Canvas* botSprite);

    void setHour24(bool v)   { _hour24 = v; }
    void setBattery(uint8_t pct);
    void setCharging(bool v) { _charging = v; }
    void setSignal(int8_t bars) { _signal = (bars < -1) ? -1 : (bars > 4 ? 4 : bars); }

    void update(uint32_t nowMs);   // read RTC, feed the bot battery (droop only)
    void draw();                   // clock + status row (call after bot.draw())

    botux::BotUx& bot() { return _bot; }

    uint8_t hour()   const { return _hh; }
    uint8_t minute() const { return _mm; }
    uint8_t second() const { return _ss; }

private:
    void _drawSignal(uint16_t accent, uint16_t dim);
    void _drawBattery(uint16_t accent, uint16_t warn);
    void _drawClock(uint16_t accent, uint16_t text);
    static void _drawBolt(M5Canvas* cv, int16_t x, int16_t y, uint16_t c);

    M5Canvas* _cv = nullptr;
    botux::BotUx _bot;

    uint32_t _now = 0;
    uint8_t  _hh = 0, _mm = 0, _ss = 0;
    bool     _hour24 = true;
    uint8_t  _battery = 100;
    bool     _charging = false;
    int8_t   _signal = -1;
};
