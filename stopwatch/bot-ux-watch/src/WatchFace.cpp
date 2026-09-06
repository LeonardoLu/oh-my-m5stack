#include "WatchFace.h"
#include "CalendarMath.h"
#include <UxRender.h>
#include <UxText.h>
#include <stdio.h>
#include <string.h>

namespace {
const char* const kWeekdays[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* const kWeekdaysZh[7] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};

void centered(M5Canvas& canvas, const char* text, int y, uint16_t color, const ux::Font& font) {
    ux::drawText(canvas, text, (466 - ux::textWidth(text, font)) / 2, y, color, font);
}

// Split only on UTF-8 boundaries; prefer a word boundary for Latin captions.
const char* wrapLine(const char* source, char* line, size_t capacity,
                     int maxWidth, const ux::Font& font, bool ellipsize) {
    size_t used = 0, wordEnd = 0;
    const char* cursor = source;
    while (*cursor) {
        const char* next = cursor; ux::nextCodepoint(next);
        size_t bytes = (size_t)(next - cursor);
        if (used + bytes >= capacity) break;
        memcpy(line + used, cursor, bytes); line[used + bytes] = 0;
        if (ux::textWidth(line, font) > maxWidth) { line[used] = 0; break; }
        if (*cursor == ' ' && used) wordEnd = used;
        used += bytes; cursor = next;
    }
    if (*cursor && !ellipsize && wordEnd > used / 2) { used = wordEnd; cursor = source + used; }
    line[used] = 0;
    if (*cursor && ellipsize) {
        while (used && (used + 3 >= capacity || ux::textWidth(line, font) + ux::textWidth("...", font) > maxWidth)) {
            --used;
            while (used && ((unsigned char)line[used] & 0xC0) == 0x80) --used;
            line[used] = 0;
        }
        if (used + 3 < capacity) memcpy(line + used, "...", 4);
    }
    while (*cursor == ' ') ++cursor;
    return cursor;
}
}

bool WatchFace::begin(M5Canvas* botSprite) {
    _bot.begin(botSprite);
    _hud.setColorDepth(16);
    _ready = _hud.createSprite(466, 90) != nullptr;
    _drawInvalid = true;
    return _ready;
}

void WatchFace::setLayout(bool swap, bool showDescription, uint8_t language) {
    language = language == 1 ? 1 : 0;
    if (_swap != swap || _showDescription != showDescription || _language != language) _drawInvalid = true;
    _swap = swap; _showDescription = showDescription; _language = language;
}

void WatchFace::setBattery(uint8_t pct) { _battery = pct > 100 ? 100 : pct; }

void WatchFace::update(uint32_t nowMs) {
    _now = nowMs;
    if (_lastRtcReadMs && nowMs - _lastRtcReadMs < 200) return;
    _lastRtcReadMs = nowMs;
    auto dt = M5.Rtc.getDateTime();
    _hh = (uint8_t)dt.time.hours; _mm = (uint8_t)dt.time.minutes; _ss = (uint8_t)dt.time.seconds;
    _year = dt.date.year; _month = (uint8_t)dt.date.month; _day = (uint8_t)dt.date.date;
    _weekDay = watchcalendar::weekDay(_year, _month, _day);
    _bot.setBattery(_battery); _bot.setBatteryVisible(false); _bot.setSignal(-1);
}

void WatchFace::draw(lgfx::LovyanGFX* target, uint16_t bg, uint16_t ink, uint16_t muted,
                     uint16_t accent, uint16_t panel, uint16_t warning, float statusProgress) {
    if (!_ready || !target) return;
    const uint16_t colors[] = {bg, ink, muted, accent, panel, warning};
    bool invalid = _drawInvalid || target != _lastTarget || memcmp(colors, _colors, sizeof(colors));
    bool clockDirty = invalid || _drawHour != _hh || _drawMinute != _mm || _drawYear != _year
        || (_showSeconds && _drawSecond != _ss) || _drawDay != _day || _drawMonth != _month;
    char description[sizeof(_description)] = {};
    _bot.describe(description, sizeof(description), _language ? botux::BotUx::Language::Chinese : botux::BotUx::Language::English);
    bool descriptionDirty = invalid || strcmp(description, _description);
    memcpy(_description, description, sizeof(_description));
    bool panelVisible = statusProgress > 0.0f;
    for (int band = 0; band < 2; ++band) {
        bool top = band == 0, clock = top == _swap;
        bool dirty = clock ? clockDirty : (invalid || (_showDescription && descriptionDirty));
        if (top && (panelVisible || _panelWasVisible)) dirty = true;
        if (!dirty) continue;
        _hud.fillSprite(bg);
        if (!(top && panelVisible)) {
            if (clock) _drawClock(top, ink, muted);
            else if (_showDescription) _drawDescription(top, muted);
        }
        if (top && panelVisible) _drawBatteryPanel(ink, muted, panel, warning, statusProgress);
        _hud.pushSprite(target, 0, top ? 0 : 376);
    }
    _drawHour = _hh; _drawMinute = _mm; _drawSecond = _ss;
    _drawDay = _day; _drawMonth = _month; _drawYear = _year;
    _panelWasVisible = panelVisible; _lastTarget = target;
    memcpy(_colors, colors, sizeof(colors)); _drawInvalid = false;
}

void WatchFace::_drawClock(bool top, uint16_t ink, uint16_t muted) {
    uint8_t h = _hour24 ? _hh : (_hh % 12 ? _hh % 12 : 12);
    char time[24];
    const char* suffix = _hour24 ? "" : (_hh >= 12 ? " PM" : " AM");
    if (_showSeconds) snprintf(time, sizeof(time), "%02u:%02u:%02u%s", h, _mm, _ss, suffix);
    else snprintf(time, sizeof(time), "%02u:%02u%s", h, _mm, suffix);
    const ux::Font* font = top ? &ux::Latin24 : &ux::Clock36;
    if (ux::textWidth(time, *font) > (top ? 240 : 300)) font = &ux::Latin24;
    centered(_hud, time, top ? 30 : 6, ink, *font);
    char date[48];
    const char* weekday = _language ? kWeekdaysZh[_weekDay % 7] : kWeekdays[_weekDay % 7];
    snprintf(date, sizeof(date), "%04d/%02u/%02u %s", (int)_year, _month, _day, weekday);
    centered(_hud, date, top ? 62 : 57, muted, _language ? ux::Cjk18 : ux::Latin18);
}

void WatchFace::_drawDescription(bool top, uint16_t ink) {
    const ux::Font& font = _language ? ux::Cjk18 : ux::Latin18;
    char first[96] = {}, second[96] = {};
    int maxWidth = 250;
    const char* rest = wrapLine(_description, first, sizeof(first), maxWidth, font, false);
    wrapLine(rest, second, sizeof(second), top ? 250 : 225, font, true);
    centered(_hud, first, top ? 30 : 8, ink, font);
    if (second[0]) centered(_hud, second, top ? 56 : 34, ink, font);
}

void WatchFace::_drawBatteryPanel(uint16_t ink, uint16_t muted, uint16_t panel,
                                  uint16_t warning, float progress) {
    progress = ux::clamp(progress, 0, 1);
    float eased = 1 - (1 - progress) * (1 - progress) * (1 - progress);
    float y = -64 + eased * 84;
    const int x = 137, w = 192, h = 62;
    uint16_t phase = (uint16_t)(_now % 1600);
    uint8_t glow = (uint8_t)(178 + (phase < 800 ? phase : 1600 - phase) * 70 / 800);
    uint16_t charge = botux::rgb565(0x42, glow, 0x78);
    ux::roundRect(_hud, x, y, w, h, 24, panel);
    ux::strokeRoundRect(_hud, x, y, w, h, 24, _charging ? charge : muted);
    char pct[8]; snprintf(pct, sizeof(pct), "%u%%", (unsigned)_battery);
    ux::drawText(_hud, pct, x + 22, (int)y + 19, _battery <= 15 ? warning : ink, ux::Latin24);
    float bx = x + 108, by = y + 21;
    ux::strokeRoundRect(_hud, bx, by, 44, 21, 5, ink, 1.5f);
    ux::roundRect(_hud, bx + 45, by + 6, 4, 9, 2, ink);
    float fill = _battery * 38.0f / 100;
    if (fill > 0) ux::roundRect(_hud, bx + 3, by + 3, fill, 15, 3, _charging ? charge : ink);
    if (_charging) {
        // A green bolt beside the cell remains readable at every fill level.
        ux::line(_hud, bx + 63, by + 2, bx + 56, by + 11, 2.5f, charge);
        ux::line(_hud, bx + 56, by + 11, bx + 62, by + 10, 2.5f, charge);
        ux::line(_hud, bx + 62, by + 10, bx + 56, by + 19, 2.5f, charge);
    }
}
