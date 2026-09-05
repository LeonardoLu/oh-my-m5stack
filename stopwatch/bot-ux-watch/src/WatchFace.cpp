#include "WatchFace.h"
#include <stdio.h>
#include <string.h>

void WatchFace::begin(M5Canvas* cv) {
    _cv = cv;
    _bot.begin(cv);
}

void WatchFace::setBattery(uint8_t pct) {
    if (pct > 100) pct = 100;
    _battery = pct;
}

void WatchFace::update(uint32_t nowMs) {
    _now = nowMs;
    auto dt = M5.Rtc.getDateTime();
    _hh = (uint8_t)dt.time.hours;
    _mm = (uint8_t)dt.time.minutes;
    _ss = (uint8_t)dt.time.seconds;

    // Feed the bot the battery for its low-battery droop, but hide its own
    // battery bar — the watch draws its own (with % and a charging bolt).
    _bot.setBattery(_battery);
    _bot.setBatteryVisible(false);
}

void WatchFace::draw() {
    const botux::BotUx::Style& st = _bot.style();
    _drawSignal(st.accentColor, botux::rgb565(0x30, 0x34, 0x3C));
    _drawBattery(st.accentColor, botux::rgb565(0xFF, 0x4D, 0x4D));
    _drawClock(st.accentColor, st.eyeColor);
}

void WatchFace::_drawSignal(uint16_t accent, uint16_t dim) {
    if (_signal < 0) return;
    for (int i = 0; i < 4; i++) {
        int16_t bh = 3 + i * 2;
        int16_t bx = 4 + i * 5;
        int16_t by = 14 - bh;
        _cv->fillRect(bx, by, 4, bh, (i < _signal) ? accent : dim);
    }
}

void WatchFace::_drawBattery(uint16_t accent, uint16_t warn) {
    bool low = (_battery <= 15);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)_battery);
    int16_t textW = (int16_t)(strlen(buf) * 6);

    _cv->setTextDatum(top_right);
    _cv->setTextSize(1.0f);
    if (_charging) _drawBolt(_cv, 131 - textW - 10, 3, accent);

    if (low && ((_now / 500) % 2) != 0) return;   // blink off half-cycle

    _cv->setTextColor(low ? warn : accent);
    _cv->drawString(buf, 131, 3);
}

void WatchFace::_drawClock(uint16_t accent, uint16_t text) {
    uint8_t h = _hh;
    char buf[8];
    if (_hour24) {
        snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)h, (unsigned)_mm);
    } else {
        uint8_t h12 = h % 12;
        if (h12 == 0) h12 = 12;
        snprintf(buf, sizeof(buf), "%u:%02u", (unsigned)h12, (unsigned)_mm);
    }

    _cv->setTextDatum(middle_center);
    _cv->setTextSize(3.0f);
    _cv->setTextColor(accent);
    _cv->drawString(buf, _cv->width() / 2, 172);

    if (!_hour24) {
        _cv->setTextSize(1.0f);
        _cv->setTextColor(text);
        _cv->drawString((h >= 12) ? "PM" : "AM", _cv->width() / 2, 196);
    }
}

void WatchFace::_drawBolt(M5Canvas* cv, int16_t x, int16_t y, uint16_t c) {
    // 8x12 zig-zag lightning bolt (charging indicator)
    cv->fillTriangle(x + 5, y + 0, x + 1, y + 5, x + 5, y + 5, c);
    cv->fillTriangle(x + 5, y + 5, x + 8, y + 5, x + 3, y + 12, c);
}
