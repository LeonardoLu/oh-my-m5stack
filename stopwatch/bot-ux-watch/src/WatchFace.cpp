#include "WatchFace.h"
#include "CalendarMath.h"
#include <stdio.h>
#include <string.h>

namespace {
const char* const kWeekdays[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
const char* const kMonths[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
}

void WatchFace::begin(M5Canvas* botSprite) {
    _bot.begin(botSprite);
}

void WatchFace::setBattery(uint8_t pct) {
    if (pct > 100) pct = 100;
    _battery = pct;
}

void WatchFace::update(uint32_t nowMs) {
    _now = nowMs;
    if (_lastRtcReadMs && nowMs - _lastRtcReadMs < 200) return;
    _lastRtcReadMs = nowMs;
    auto dt = M5.Rtc.getDateTime();
    _hh = (uint8_t)dt.time.hours;
    _mm = (uint8_t)dt.time.minutes;
    _ss = (uint8_t)dt.time.seconds;
    _year = dt.date.year;
    _month = (uint8_t)dt.date.month;
    _day = (uint8_t)dt.date.date;
    // RX8130's weekday register can be stale even when its numeric date is valid.
    _weekDay = watchcalendar::weekDay(_year, _month, _day);

    // Feed the bot the battery for its low-battery droop, but hide its own
    // battery icon — the watch draws its own (with % and a charging bolt).
    _bot.setBattery(_battery);
    _bot.setBatteryVisible(false);
    _bot.setSignal(-1);
}

void WatchFace::draw(lgfx::LovyanGFX* target, uint16_t bg, uint16_t ink, uint16_t muted,
                     uint16_t accent, uint16_t panel, uint16_t warning, float statusProgress) {
    bool clockDirty = _drawInvalid || _drawHour != _hh || _drawMinute != _mm || _drawYear != _year
        || (_showSeconds && _drawSecond != _ss) || _drawDay != _day || _drawMonth != _month;
    if (clockDirty) _drawClock(target, bg, ink, muted, accent);
    if (statusProgress > 0.0f || _panelWasVisible) target->fillRect(120, 0, 226, 66, bg);
    if (statusProgress > 0.0f) _drawBatteryPanel(target, ink, muted, panel, warning, statusProgress);
    _panelWasVisible = statusProgress > 0.0f;

    _drawHour = _hh;
    _drawMinute = _mm;
    _drawSecond = _ss;
    _drawDay = _day;
    _drawMonth = _month;
    _drawYear = _year;
    _drawBattery = _battery;
    _drawCharging = _charging;
    _drawInvalid = false;
}

void WatchFace::_drawClock(lgfx::LovyanGFX* target, uint16_t bg, uint16_t ink,
                           uint16_t muted, uint16_t accent) {
    uint8_t h = _hh;
    char buf[20];
    if (_hour24) {
        if (_showSeconds)
            snprintf(buf, sizeof(buf), "%02u:%02u:%02u", (unsigned)h, (unsigned)_mm, (unsigned)_ss);
        else
            snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)h, (unsigned)_mm);
    } else {
        uint8_t h12 = h % 12;
        if (h12 == 0) h12 = 12;
        if (_showSeconds)
            snprintf(buf, sizeof(buf), "%u:%02u:%02u %s", (unsigned)h12, (unsigned)_mm,
                     (unsigned)_ss, (h >= 12) ? "PM" : "AM");
        else
            snprintf(buf, sizeof(buf), "%u:%02u %s", (unsigned)h12, (unsigned)_mm,
                     (h >= 12) ? "PM" : "AM");
    }

    target->fillRect(88, 370, 290, 91, bg);
    target->setFont(&fonts::FreeSansBold18pt7b);
    target->setTextDatum(middle_center);
    target->setTextSize(1.0f);
    target->setTextColor(ink);
    target->drawString(buf, 233, 394);

    char detail[40];
    const char* wd = (_weekDay < 7) ? kWeekdays[_weekDay] : "---";
    const char* mo = (_month >= 1 && _month <= 12) ? kMonths[_month - 1] : "---";
    snprintf(detail, sizeof(detail), "%s  %02u %s %d", wd, (unsigned)_day, mo, (int)_year);
    target->setFont(&fonts::FreeSansBold9pt7b);
    target->setTextColor(muted);
    target->drawString(detail, 233, 437);
}

void WatchFace::_drawBatteryPanel(lgfx::LovyanGFX* target, uint16_t ink, uint16_t muted,
                                  uint16_t panel, uint16_t warning, float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    float eased = 1.0f - (1.0f - progress) * (1.0f - progress) * (1.0f - progress);
    int16_t y = (int16_t)(-82.0f + eased * 74.0f);
    const int16_t x = 137, w = 192, h = 65;
    uint16_t phase = (uint16_t)(_now % 1600);
    uint8_t glow = (uint8_t)(178 + (phase < 800 ? phase : 1600 - phase) * 70 / 800);
    uint16_t charge = botux::rgb565(0x42, glow, 0x78);
    target->fillRoundRect(x, y, w, h, 28, panel);
    target->drawRoundRect(x, y, w, h, 28, _charging ? charge : muted);

    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", (unsigned)_battery);
    target->setFont(&fonts::FreeSansBold12pt7b);
    target->setTextSize(1.0f);
    target->setTextDatum(middle_left);
    target->setTextColor(_battery <= 15 ? warning : ink);
    target->drawString(pct, x + 25, y + 38);

    int16_t bx = x + 103, by = y + 25;
    target->drawRoundRect(bx, by, 45, 20, 5, ink);
    target->fillRoundRect(bx + 47, by + 6, 4, 8, 2, ink);
    int16_t fill = (int16_t)(_battery * 39 / 100);
    if (fill > 0) target->fillRoundRect(bx + 3, by + 3, fill, 14, 3, _charging ? charge : ink);
    if (_charging) _drawBolt(target, bx + 17, by + 4, panel);
}

void WatchFace::_drawBolt(lgfx::LovyanGFX* target, int16_t x, int16_t y, uint16_t c) {
    // 8x12 zig-zag lightning bolt (charging indicator)
    target->fillTriangle(x + 5, y + 0, x + 1, y + 5, x + 5, y + 5, c);
    target->fillTriangle(x + 5, y + 5, x + 8, y + 5, x + 3, y + 12, c);
}
