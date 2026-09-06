#include "Settings.h"

#include <M5Unified.h>
#include <Preferences.h>

namespace {
constexpr uint16_t kPaper = botux::rgb565(238, 236, 229);
constexpr uint16_t kWarm = botux::rgb565(244, 232, 215);
constexpr uint16_t kGraphite = botux::rgb565(38, 40, 45);
constexpr uint16_t kInk = botux::rgb565(33, 36, 42);
constexpr uint16_t kLine = botux::rgb565(199, 197, 190);
constexpr uint16_t kBlue = botux::rgb565(60, 124, 232);
constexpr uint8_t kDisplayBrightness[5] = {40, 88, 144, 204, 255};
constexpr int16_t kRowY[4] = {40, 80, 120, 160};

const char* animationName(uint8_t value)
{
    static const char* names[] = {"AUTO", "CALM", "CURIOUS", "ORBIT",
                                  "BOUNCE", "GLITCH", "WAVE", "SPARKLE"};
    return names[value > 7 ? 0 : value];
}

const char* onOff(uint8_t value) { return value ? "ON" : "OFF"; }

const char* ledName(uint8_t value)
{
    static const char* names[] = {"OFF", "LOW", "MED", "HIGH"};
    return names[value > 3 ? 2 : value];
}

const char* motionName(uint8_t value)
{
    static const char* names[] = {"OFF", "LOW", "MED", "HIGH", "MAX"};
    return names[value > 4 ? 2 : value];
}

void drawRow(M5Canvas& cv, int16_t y, const char* label, const char* value,
             bool down, uint16_t bg, uint16_t fg, uint16_t accent)
{
    const uint16_t pressedFill = fg == kInk ? botux::rgb565(218, 224, 230)
                                          : botux::rgb565(73, 79, 90);
    const uint16_t fill = down ? pressedFill : bg;
    cv.fillRoundRect(6, y + 2, 308, 36, 7, fill);
    cv.drawRoundRect(6, y + 2, 308, 36, 7, kLine);
    cv.setTextSize(1.0f);
    cv.setTextDatum(middle_left);
    cv.setTextColor(fg);
    cv.drawString(label, 16, y + 20);
    cv.setTextDatum(middle_right);
    cv.setTextColor(accent);
    cv.drawString(value, 304, y + 20);
}
}

const char* Settings::themeName(uint8_t value)
{
    static const char* names[kThemeCount] = {"PAPER", "WARM", "DARK"};
    return names[value < kThemeCount ? value : 0];
}

botux::BotUx::Style Settings::themeStyle(uint8_t value)
{
    botux::BotUx::Style style;
    style.bgColor = value == 1 ? kWarm : (value == 2 ? kGraphite : kPaper);
    style.bodyColor = botux::rgb565(252, 252, 250);
    style.accentColor = value == 1 ? botux::rgb565(224, 112, 55) : kBlue;
    style.eyeColor = kInk;
    style.pupilColor = botux::rgb565(12, 14, 18);
    style.mouthColor = kInk;
    style.blushColor = botux::rgb565(238, 130, 126);
    style.eyeStyle = botux::BotUx::EyeStyle::Oval;
    style.bodyStyle = botux::BotUx::BodyStyle::Round;
    return style;
}

void Settings::begin()
{
    Preferences prefs;
    prefs.begin("codexmicro", true);
    _data.theme = prefs.getUChar("theme", 0);
    _data.audio = prefs.getUChar("audio", 1);
    _data.brightness = prefs.getUChar("display", 3);
    _data.reducedMotion = prefs.getUChar("reduce", 0);
    _data.ledBrightness = prefs.getUChar("led", 2);
    _data.animation = prefs.getUChar("anim", 0);
    _data.motion = prefs.getUChar("motion", 2);
    prefs.end();
    if (_data.theme >= kThemeCount) _data.theme = 0;
    if (_data.audio > 1) _data.audio = 1;
    if (_data.brightness < 1 || _data.brightness > 5) _data.brightness = 3;
    if (_data.reducedMotion > 1) _data.reducedMotion = 0;
    if (_data.ledBrightness > 3) _data.ledBrightness = 2;
    if (_data.animation > 7) _data.animation = 0;
    if (_data.motion > 4) _data.motion = 2;
}

void Settings::_save() const
{
    Preferences prefs;
    prefs.begin("codexmicro", false);
    prefs.putUChar("theme", _data.theme);
    prefs.putUChar("audio", _data.audio);
    prefs.putUChar("display", _data.brightness);
    prefs.putUChar("reduce", _data.reducedMotion);
    prefs.putUChar("led", _data.ledBrightness);
    prefs.putUChar("anim", _data.animation);
    prefs.putUChar("motion", _data.motion);
    prefs.end();
}

void Settings::apply(botux::BotUx& bot) const
{
    bot.setStyle(themeStyle(_data.theme));
    bot.setAnimation(static_cast<botux::BotUx::Animation>(_data.animation));
    bot.setAnimationSpeed(1.0f);
    bot.setMotionAmount(_data.motion * 0.5f);
    bot.setReducedMotion(_data.reducedMotion != 0);
    applyBrightness();
}

void Settings::applyBrightness() const
{
    M5.Display.setBrightness(kDisplayBrightness[_data.brightness - 1]);
}

void Settings::open(uint8_t page)
{
    _open = true;
    _page = page > 1 ? 0 : page;
    _pressed = -1;
}

void Settings::close()
{
    _open = false;
    _pressed = -1;
}

int8_t Settings::_hit(int16_t x, int16_t y) const
{
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return -1;
    if (x >= 244 && y >= 0 && y < 40) return 0;
    for (int8_t i = 0; i < (_page == 0 ? 4 : 3); ++i)
        if (y >= kRowY[i] && y < kRowY[i] + 40) return i + 1;
    if (y >= 200 && y < 240) return 5;
    return -1;
}

void Settings::touchBegin(int16_t x, int16_t y) { _pressed = _hit(x, y); }

bool Settings::touchMove(int16_t x, int16_t y)
{
    if (_pressed < 0 || _hit(x, y) == _pressed) return false;
    _pressed = -1;
    return true;
}

void Settings::touchEnd(int16_t x, int16_t y)
{
    const int8_t released = _hit(x, y);
    if (_pressed >= 0 && released == _pressed) _activate(_pressed);
    _pressed = -1;
}

void Settings::_activate(int8_t target)
{
    if (target == 0) { close(); return; }
    if (target == 5) { _page ^= 1; return; }
    if (_page == 0)
    {
        if (target == 1) _data.animation = (_data.animation + 1) % 8;
        else if (target == 2) _data.brightness = (_data.brightness % 5) + 1;
        else if (target == 3) _data.audio ^= 1;
        else if (target == 4) _data.theme = (_data.theme + 1) % kThemeCount;
    }
    else
    {
        if (target == 1) _data.motion = (_data.motion + 1) % 5;
        else if (target == 2) _data.reducedMotion ^= 1;
        else if (target == 3) _data.ledBrightness = (_data.ledBrightness + 1) % 4;
    }
    _save();
    applyBrightness();
}

void Settings::draw(M5Canvas& cv, M5Canvas& botSprite) const
{
    const botux::BotUx::Style style = themeStyle(_data.theme);
    const uint16_t fg = _data.theme == 2 ? botux::rgb565(238, 238, 234) : kInk;
    const uint16_t rowBg = _data.theme == 2 ? botux::rgb565(57, 60, 66) : botux::rgb565(250, 249, 245);
    cv.fillSprite(style.bgColor);
    botSprite.pushSprite(&cv, 2, 0);
    cv.setTextSize(1.0f);
    cv.setTextDatum(middle_left);
    cv.setTextColor(fg);
    cv.drawString(_page ? "SETTINGS 2/2" : "SETTINGS 1/2", 48, 20);
    cv.fillRoundRect(244, 2, 72, 36, 8, style.accentColor);
    cv.setTextDatum(middle_center);
    cv.setTextColor(botux::rgb565(255, 255, 255));
    cv.drawString("DONE", 280, 20);

    if (_page == 0)
    {
        char level[2] = {static_cast<char>('0' + _data.brightness), '\0'};
        drawRow(cv, kRowY[0], "BOT ANIMATION", animationName(_data.animation), _pressed == 1, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[1], "DISPLAY", level, _pressed == 2, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[2], "AUDIO FEEDBACK", onOff(_data.audio), _pressed == 3, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[3], "THEME", themeName(_data.theme), _pressed == 4, rowBg, fg, style.accentColor);
    }
    else
    {
        drawRow(cv, kRowY[0], "IMU MOTION", motionName(_data.motion), _pressed == 1, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[1], "REDUCED MOTION", onOff(_data.reducedMotion), _pressed == 2, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[2], "BOTTOM LEDs", ledName(_data.ledBrightness), _pressed == 3, rowBg, fg, style.accentColor);
        cv.setTextDatum(middle_left);
        cv.setTextColor(fg);
        cv.drawString("BLE HID / CODEX DESKTOP", 16, 181);
    }

    cv.fillRoundRect(6, 202, 308, 36, 7, _pressed == 5 ? style.accentColor : rowBg);
    cv.setTextDatum(middle_center);
    cv.setTextColor(_pressed == 5 ? botux::rgb565(255, 255, 255) : fg);
    cv.drawString(_page ? "<  GENERAL" : "MORE SETTINGS  >", 160, 220);
}
