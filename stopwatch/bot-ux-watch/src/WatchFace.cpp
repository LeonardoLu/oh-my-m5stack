#include "WatchFace.h"
#include <stdio.h>
#include <string.h>

namespace {
const char* const kWeekdays[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
const char* const kMonths[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
}

void WatchFace::begin(M5Canvas* cv, M5Canvas* botSprite) {
    _cv = cv;
    _bot.begin(botSprite);
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
    _year = dt.date.year;
    _month = (uint8_t)dt.date.month;
    _day = (uint8_t)dt.date.date;
    _weekDay = (uint8_t)dt.date.weekDay;

    // Feed the bot the battery for its low-battery droop, but hide its own
    // battery icon — the watch draws its own (with % and a charging bolt).
    _bot.setBattery(_battery);
    _bot.setBatteryVisible(false);
    _bot.setSignal(-1);
}

void WatchFace::draw(uint16_t ink, uint16_t muted, uint16_t accent,
                     uint16_t panel, uint16_t warning) {
    _drawDate(muted);
    _drawBattery(ink, accent, warning);
    _drawClock(ink, muted, accent);
    _drawSetPill(ink, accent, panel);
}

void WatchFace::_drawDate(uint16_t muted) {
    char buf[16];
    const char* wd = (_weekDay < 7) ? kWeekdays[_weekDay] : "---";
    const char* mo = (_month >= 1 && _month <= 12) ? kMonths[_month - 1] : "---";
    snprintf(buf, sizeof(buf), "%s %02u %s", wd, (unsigned)_day, mo);
    _cv->setTextDatum(middle_left);
    _cv->setTextSize(1.35f);
    _cv->setTextColor(muted);
    _cv->drawString(buf, 112, 50);
}

void WatchFace::_drawBattery(uint16_t ink, uint16_t accent, uint16_t warning) {
    bool low = (_battery <= 15);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)_battery);

    _cv->setTextDatum(middle_right);
    _cv->setTextSize(1.35f);
    _cv->setTextColor(low ? warning : ink);
    _cv->drawString(buf, 358, 50);
    if (_charging) _drawBolt(_cv, 304, 43, accent);
}

void WatchFace::_drawClock(uint16_t ink, uint16_t muted, uint16_t accent) {
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
    _cv->setTextSize(5.0f);
    _cv->setTextColor(ink);
    _cv->drawString(buf, _cv->width() / 2, 322);

    char detail[16];
    if (_showSeconds && !_hour24)
        snprintf(detail, sizeof(detail), "%02u SEC  %s", (unsigned)_ss, (h >= 12) ? "PM" : "AM");
    else if (_showSeconds)
        snprintf(detail, sizeof(detail), "%02u SECONDS", (unsigned)_ss);
    else if (!_hour24)
        snprintf(detail, sizeof(detail), "%s", (h >= 12) ? "PM" : "AM");
    else
        detail[0] = '\0';
    _cv->setTextSize(1.25f);
    _cv->setTextColor(_showSeconds ? accent : muted);
    if (detail[0]) _cv->drawString(detail, _cv->width() / 2, 368);
}

void WatchFace::_drawSetPill(uint16_t ink, uint16_t accent, uint16_t panel) {
    _cv->fillRoundRect(173, 393, 120, 44, 22, panel);
    _cv->drawRoundRect(173, 393, 120, 44, 22, accent);
    _cv->setTextDatum(middle_center);
    _cv->setTextSize(1.45f);
    _cv->setTextColor(ink);
    _cv->drawString("SET", 233, 415);
}

void WatchFace::_drawBolt(M5Canvas* cv, int16_t x, int16_t y, uint16_t c) {
    // 8x12 zig-zag lightning bolt (charging indicator)
    cv->fillTriangle(x + 5, y + 0, x + 1, y + 5, x + 5, y + 5, c);
    cv->fillTriangle(x + 5, y + 5, x + 8, y + 5, x + 3, y + 12, c);
}
