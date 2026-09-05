// Settings implementation. See Settings.h for the contract.
#include "Settings.h"

#include <M5Unified.h>  // M5.Display.setBrightness
#include <Preferences.h> // NVS persistence

namespace {

constexpr int16_t kW = 320;

// list row bands (y ranges)
constexpr int16_t kRowY[4] = {40, 72, 104, 136};
constexpr int16_t kRowH = 28;
constexpr int16_t kDoneY = 180;
constexpr int16_t kDoneH = 36;

const uint8_t kBright[5] = {32, 88, 144, 200, 255}; // level 1..5 → 0..255

// dim "off" segment color (unlit slider / empty bars)
uint16_t dim(uint8_t r, uint8_t g, uint8_t b) { return botux::rgb565(r, g, b); }

void drawRow(M5Canvas* cv, const botux::Style& st, int16_t y, const char* label, const char* value)
{
    cv->fillRoundRect(8, y, kW - 16, kRowH, 6, st.bodyColor);
    cv->setTextDatum(middle_left);
    cv->setTextSize(1.0f);
    cv->setTextColor(st.eyeColor);
    cv->drawString(label, 18, y + kRowH / 2);
    cv->setTextDatum(middle_right);
    cv->setTextColor(st.accentColor);
    cv->drawString(value, kW - 8, y + kRowH / 2);
}

} // namespace

const char* Settings::themeName(int i)
{
    static const char* names[kThemeCount] = {"Grok Teal", "Sunset", "Mono"};
    int n = i % kThemeCount;
    if (n < 0) n += kThemeCount;
    return names[n];
}

botux::Style Settings::themeStyle(int i)
{
    botux::Style s;
    int n = i % kThemeCount;
    if (n < 0) n += kThemeCount;

    switch (n)
    {
        default: // 0 — Grok Teal
            s.bgColor     = botux::rgb565(0x0A, 0x0E, 0x14);
            s.bodyColor   = botux::rgb565(0x2A, 0x2E, 0x38);
            s.accentColor = botux::rgb565(0x2E, 0xD9, 0xC8);
            s.eyeColor    = botux::rgb565(0xFF, 0xFF, 0xFF);
            s.pupilColor  = botux::rgb565(0x0A, 0x12, 0x22);
            s.mouthColor  = botux::rgb565(0xFF, 0xFF, 0xFF);
            s.blushColor  = botux::rgb565(0xFF, 0x8A, 0xA0);
            s.eyeStyle    = botux::BotUx::EyeStyle::Round;
            s.bodyStyle   = botux::BotUx::BodyStyle::Round;
            break;
        case 1: // Sunset
            s.bgColor     = botux::rgb565(0x1A, 0x0F, 0x14);
            s.bodyColor   = botux::rgb565(0x4A, 0x2A, 0x30);
            s.accentColor = botux::rgb565(0xFF, 0x9F, 0x43);
            s.eyeColor    = botux::rgb565(0xFF, 0xE8, 0xC8);
            s.pupilColor  = botux::rgb565(0x2A, 0x15, 0x20);
            s.mouthColor  = botux::rgb565(0xFF, 0xD9, 0xA0);
            s.blushColor  = botux::rgb565(0xFF, 0x6B, 0x81);
            s.eyeStyle    = botux::BotUx::EyeStyle::Oval;
            s.bodyStyle   = botux::BotUx::BodyStyle::RoundedSquare;
            break;
        case 2: // Mono
            s.bgColor     = botux::rgb565(0x0F, 0x0F, 0x0F);
            s.bodyColor   = botux::rgb565(0x2E, 0x2E, 0x2E);
            s.accentColor = botux::rgb565(0xE6, 0xE6, 0xE6);
            s.eyeColor    = botux::rgb565(0xFF, 0xFF, 0xFF);
            s.pupilColor  = botux::rgb565(0x11, 0x11, 0x11);
            s.mouthColor  = botux::rgb565(0xFF, 0xFF, 0xFF);
            s.blushColor  = botux::rgb565(0x77, 0x77, 0x77);
            s.eyeStyle    = botux::BotUx::EyeStyle::Square;
            s.bodyStyle   = botux::BotUx::BodyStyle::Hexagon;
            break;
    }
    return s;
}

void Settings::begin()
{
    Preferences p;
    p.begin("codex", true); // read-only
    _data.kbdLayout  = p.getUChar("kbd", 0);
    _data.theme      = p.getUChar("theme", 0);
    _data.haptics    = p.getUChar("hapt", 1);
    _data.brightness = p.getUChar("bright", 3);
    p.end();

    // clamp
    if (_data.kbdLayout > 1) _data.kbdLayout = 0;
    if (_data.theme > 2) _data.theme = 0;
    if (_data.haptics > 1) _data.haptics = 1;
    if (_data.brightness < 1 || _data.brightness > 5) _data.brightness = 3;
}

void Settings::_save()
{
    Preferences p;
    p.begin("codex", false);
    p.putUChar("kbd", _data.kbdLayout);
    p.putUChar("theme", _data.theme);
    p.putUChar("hapt", _data.haptics);
    p.putUChar("bright", _data.brightness);
    p.end();
}

void Settings::apply(botux::BotUx* bot)
{
    bot->setStyle(themeStyle(_data.theme));
    applyBrightness();
}

void Settings::applyBrightness()
{
    uint8_t lvl = _data.brightness;
    if (lvl < 1) lvl = 1;
    if (lvl > 5) lvl = 5;
    M5.Display.setBrightness(kBright[lvl - 1]);
}

void Settings::cycleTheme(int dir)
{
    int t = (int)_data.theme + dir;
    while (t < 0) t += kThemeCount;
    while (t >= kThemeCount) t -= kThemeCount;
    _data.theme = (uint8_t)t;
    _save();
}

void Settings::open()
{
    _open = true;
    _sub = List;
    _down = false;
    _dragRow = -1;
}

void Settings::close()
{
    _open = false;
    _down = false;
    _dragRow = -1;
}

int16_t Settings::_rowAt(int16_t x, int16_t y) const
{
    (void)x;
    for (int i = 0; i < 4; i++)
    {
        if (y >= kRowY[i] && y < kRowY[i] + kRowH) return i;
    }
    if (y >= kDoneY && y < kDoneY + kDoneH) return 4;
    return -1;
}

void Settings::_setBrightnessFromX(int16_t x)
{
    int16_t sx = x;
    if (sx < 120) sx = 120;
    if (sx > 294) sx = 294;
    int level = 1 + (sx - 120) * 5 / 175; // 0..174 → 0..4
    if (level < 1) level = 1;
    if (level > 5) level = 5;
    if (level != _data.brightness)
    {
        _data.brightness = (uint8_t)level;
        _save();
        applyBrightness();
    }
}

void Settings::touchBegin(int16_t x, int16_t y)
{
    _px = x; _py = y;
    _down = true;
    _swiped = false;
    _dragRow = (_sub == List) ? _rowAt(x, y) : -1;
    if (_dragRow == 3) _setBrightnessFromX(x); // brightness row → start drag now
}

void Settings::touchMove(int16_t x, int16_t y)
{
    if (!_down) return;
    if (_sub == Carousel)
    {
        int16_t dx = x - _px;
        if (!_swiped)
        {
            if (dx > 40)      { cycleTheme(+1); _swiped = true; }
            else if (dx < -40) { cycleTheme(-1); _swiped = true; }
        }
    }
    else if (_dragRow == 3)
    {
        _setBrightnessFromX(x);
    }
    (void)y;
}

void Settings::touchEnd(int16_t x, int16_t y)
{
    if (!_down) return;
    if (_sub == Carousel)
    {
        if (!_swiped) _sub = List; // tap = select current theme, back to list
    }
    else if (_dragRow != 3)
    {
        _handleRow(_rowAt(x, y));
    }
    _down = false;
    _dragRow = -1;
}

void Settings::_handleRow(int16_t row)
{
    switch (row)
    {
        case 0: // keyboard layout cycle
            _data.kbdLayout = (_data.kbdLayout + 1) % 2;
            _save();
            break;
        case 1: // theme → carousel
            _sub = Carousel;
            break;
        case 2: // haptics toggle
            _data.haptics = _data.haptics ? 0 : 1;
            _save();
            break;
        case 4: // Done
            close();
            break;
        default:
            break;
    }
}

void Settings::draw(M5Canvas* cv, M5Canvas* botSprite)
{
    botux::Style st = themeStyle(_data.theme);
    cv->fillSprite(st.bgColor);

    // title bar
    cv->setTextDatum(middle_center);
    cv->setTextSize(1.0f);
    cv->setTextColor(st.accentColor);
    cv->drawString("Settings", 40, 12);
    cv->setTextColor(st.eyeColor);
    cv->drawString("Done", kW - 40, 12);

    if (_sub == List)
    {
        drawRow(cv, st, kRowY[0], "Keyboard", _data.kbdLayout ? "Chips" : "QWERTY");
        drawRow(cv, st, kRowY[1], "Theme", themeName(_data.theme));
        drawRow(cv, st, kRowY[2], "Haptics", _data.haptics ? "ON" : "OFF");

        // brightness slider row
        cv->fillRoundRect(8, kRowY[3], kW - 16, kRowH, 6, st.bodyColor);
        cv->setTextDatum(middle_left);
        cv->setTextSize(1.0f);
        cv->setTextColor(st.eyeColor);
        cv->drawString("Brightness", 18, kRowY[3] + kRowH / 2);
        for (int i = 0; i < 5; i++)
        {
            uint16_t c = (i < _data.brightness) ? st.accentColor : dim(0x30, 0x34, 0x3C);
            cv->fillRoundRect(120 + i * 36, kRowY[3] + 10, 30, 8, 3, c);
        }

        // Done button
        cv->fillRoundRect(8, kDoneY, kW - 16, kDoneH, 8, st.accentColor);
        cv->setTextDatum(middle_center);
        cv->setTextColor(st.bgColor);
        cv->drawString("Done", kW / 2, kDoneY + kDoneH / 2);
    }
    else // Carousel
    {
        // three theme cards
        for (int i = 0; i < kThemeCount; i++)
        {
            bool sel = (i == _data.theme);
            cv->fillRoundRect(16 + i * 100, 32, 88, 22, 6, sel ? st.accentColor : st.bodyColor);
            cv->setTextDatum(middle_center);
            cv->setTextSize(1.0f);
            cv->setTextColor(sel ? st.bgColor : st.eyeColor);
            cv->drawString(themeName(i), 16 + i * 100 + 44, 43);
        }

        // live preview — push the (already rendered) bot sprite
        int16_t pw = botSprite->width();
        botSprite->pushSprite(cv, (kW - pw) / 2, 70);

        cv->setTextDatum(middle_center);
        cv->setTextSize(1.0f);
        cv->setTextColor(st.eyeColor);
        cv->drawString("swipe to change - tap to select", kW / 2, 220);
    }
}
