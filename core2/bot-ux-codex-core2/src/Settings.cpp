#include "Settings.h"

#include <M5Unified.h>
#include <Preferences.h>

#include <stdio.h>

namespace {
constexpr uint16_t kPaper = botux::rgb565(238, 236, 229);
constexpr uint16_t kWarm = botux::rgb565(244, 232, 215);
constexpr uint16_t kGraphite = botux::rgb565(38, 40, 45);
constexpr uint16_t kInk = botux::rgb565(33, 36, 42);
constexpr uint16_t kLine = botux::rgb565(199, 197, 190);
constexpr uint16_t kBlue = botux::rgb565(60, 124, 232);
constexpr uint8_t kDisplayBrightness[5] = {40, 88, 144, 204, 255};
constexpr int16_t kRowY[4] = {40, 80, 120, 160};
constexpr int16_t kSliderLeft = 42;
constexpr int16_t kSliderRight = 272;

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

const char* botStyleName(uint8_t value)
{
    static const char* names[] = {"CLASSIC", "SOFT", "FACET", "GOOGLY", "MINIMAL"};
    return names[value > 4 ? 0 : value];
}

const char* expressionName(uint8_t value)
{
    static const char* names[] = {"AUTO", "NEUTRAL", "CURIOUS", "FOCUSED", "JOY",
                                  "SKEPTICAL", "BASHFUL", "WINK", "DIZZY", "ALARMED"};
    return names[value > 9 ? 0 : value];
}

const char* pageName(uint8_t value)
{
    static const char* names[] = {"GENERAL", "BOT", "BOT COLOR", "MOTION"};
    return names[value < Settings::kPageCount ? value : 0];
}

const char* colorPartName(uint8_t value)
{
    static const char* names[] = {"BODY", "EYES", "ACCENT"};
    return names[value > 2 ? 0 : value];
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

void drawSlider(M5Canvas& cv, int16_t y, const char* label, uint8_t value,
                uint8_t channel, uint8_t red, uint8_t green, uint8_t blue,
                uint16_t fg, uint16_t line)
{
    cv.setTextDatum(middle_left);
    cv.setTextColor(fg);
    cv.drawString(label, 12, y + 20);
    for (int16_t x = kSliderLeft; x < kSliderRight; x += 5)
    {
        const uint8_t shade = static_cast<uint8_t>(
            (static_cast<uint32_t>(x - kSliderLeft) * 255) / (kSliderRight - kSliderLeft - 1));
        const uint8_t r = channel == 0 ? shade : red;
        const uint8_t g = channel == 1 ? shade : green;
        const uint8_t b = channel == 2 ? shade : blue;
        cv.fillRect(x, y + 9, 5, 22, botux::rgb565(r, g, b));
    }
    cv.drawRect(kSliderLeft, y + 9, kSliderRight - kSliderLeft, 22, line);
    const int16_t marker = kSliderLeft +
        static_cast<int16_t>((static_cast<uint32_t>(value) * (kSliderRight - kSliderLeft - 1)) / 255);
    cv.drawFastVLine(marker, y + 6, 28, botux::rgb565(255, 255, 255));
    cv.drawFastVLine(marker + 1, y + 6, 28, botux::rgb565(0, 0, 0));
    char number[4];
    snprintf(number, sizeof(number), "%u", static_cast<unsigned>(value));
    cv.setTextDatum(middle_right);
    cv.setTextColor(fg);
    cv.drawString(number, 314, y + 20);
}

void drawPager(M5Canvas& cv, uint8_t page, int8_t pressed, uint16_t bg,
               uint16_t fg, uint16_t accent)
{
    if (pressed == 5) cv.fillRect(0, 201, 160, 39, bg);
    if (pressed == 6) cv.fillRect(160, 201, 160, 39, bg);
    cv.drawFastHLine(0, 200, 320, accent);
    cv.setTextDatum(middle_center);
    cv.setTextColor(fg);
    cv.drawString("<", 34, 220);
    cv.drawString(">", 286, 220);
    char position[8];
    snprintf(position, sizeof(position), "%u / %u", static_cast<unsigned>(page + 1),
             static_cast<unsigned>(Settings::kPageCount));
    cv.drawString(position, 160, 220);
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

botux::BotUx::Style Settings::botStyle() const
{
    botux::BotUx::Style style = themeStyle(_data.theme);
    style.bodyColor = botux::rgb565(_data.bodyRed, _data.bodyGreen, _data.bodyBlue);
    style.eyeColor = botux::rgb565(_data.eyeRed, _data.eyeGreen, _data.eyeBlue);
    const uint16_t eyeLuma = static_cast<uint16_t>(_data.eyeRed) * 3
                           + static_cast<uint16_t>(_data.eyeGreen) * 6
                           + _data.eyeBlue;
    style.pupilColor = eyeLuma >= 1280
        ? botux::rgb565(12, 14, 18) : botux::rgb565(252, 252, 250);
    style.mouthColor = style.eyeColor;
    style.accentColor = botux::rgb565(_data.accentRed, _data.accentGreen, _data.accentBlue);
    style.blushColor = style.accentColor;
    switch (_data.botStyle)
    {
        case 1:
            style.bodyStyle = botux::BotUx::BodyStyle::RoundedSquare;
            style.eyeStyle = botux::BotUx::EyeStyle::Round;
            break;
        case 2:
            style.bodyStyle = botux::BotUx::BodyStyle::Hexagon;
            style.eyeStyle = botux::BotUx::EyeStyle::Square;
            break;
        case 3:
            style.bodyStyle = botux::BotUx::BodyStyle::Round;
            style.eyeStyle = botux::BotUx::EyeStyle::Googly;
            break;
        case 4:
            style.bodyStyle = botux::BotUx::BodyStyle::None;
            style.eyeStyle = botux::BotUx::EyeStyle::Oval;
            style.eyeSize = 1.1f;
            break;
        default: break;
    }
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
    _data.botStyle = prefs.getUChar("botstyle", 0);
    _data.expression = prefs.getUChar("expr", 0);
    _data.bodyRed = prefs.getUChar("bodyr", 252);
    _data.bodyGreen = prefs.getUChar("bodyg", 252);
    _data.bodyBlue = prefs.getUChar("bodyb", 250);
    _data.eyeRed = prefs.getUChar("eyer", 33);
    _data.eyeGreen = prefs.getUChar("eyeg", 36);
    _data.eyeBlue = prefs.getUChar("eyeb", 42);
    _data.accentRed = prefs.getUChar("accentr", 60);
    _data.accentGreen = prefs.getUChar("accentg", 124);
    _data.accentBlue = prefs.getUChar("accentb", 232);
    prefs.end();
    if (_data.theme >= kThemeCount) _data.theme = 0;
    if (_data.audio > 1) _data.audio = 1;
    if (_data.brightness < 1 || _data.brightness > 5) _data.brightness = 3;
    if (_data.reducedMotion > 1) _data.reducedMotion = 0;
    if (_data.ledBrightness > 3) _data.ledBrightness = 2;
    if (_data.animation > 7) _data.animation = 0;
    if (_data.motion > 4) _data.motion = 2;
    if (_data.botStyle > 4) _data.botStyle = 0;
    if (_data.expression > 9) _data.expression = 0;
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
    prefs.putUChar("botstyle", _data.botStyle);
    prefs.putUChar("expr", _data.expression);
    prefs.putUChar("bodyr", _data.bodyRed);
    prefs.putUChar("bodyg", _data.bodyGreen);
    prefs.putUChar("bodyb", _data.bodyBlue);
    prefs.putUChar("eyer", _data.eyeRed);
    prefs.putUChar("eyeg", _data.eyeGreen);
    prefs.putUChar("eyeb", _data.eyeBlue);
    prefs.putUChar("accentr", _data.accentRed);
    prefs.putUChar("accentg", _data.accentGreen);
    prefs.putUChar("accentb", _data.accentBlue);
    prefs.end();
}

void Settings::apply(botux::BotUx& bot) const
{
    bot.setStyle(botStyle());
    bot.setExpression(static_cast<botux::BotUx::Expression>(_data.expression), 650);
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
    _page = page < kPageCount ? page : 0;
    _pressed = -1;
    _colorChanged = false;
}

void Settings::close()
{
    _open = false;
    _pressed = -1;
    _colorChanged = false;
}

int8_t Settings::_hit(int16_t x, int16_t y) const
{
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return -1;
    if (x >= 244 && y >= 0 && y < 40) return 0;
    const int8_t rowCount = _page == 3 ? 2 : 4;
    for (int8_t i = 0; i < rowCount; ++i)
        if (y >= kRowY[i] && y < kRowY[i] + 40) return i + 1;
    if (y >= 200 && y < 240) return x < 160 ? 5 : 6;
    return -1;
}

uint8_t& Settings::_colorChannel(uint8_t channel)
{
    uint8_t* colors[] = {
        &_data.bodyRed, &_data.bodyGreen, &_data.bodyBlue,
        &_data.eyeRed, &_data.eyeGreen, &_data.eyeBlue,
        &_data.accentRed, &_data.accentGreen, &_data.accentBlue,
    };
    return *colors[_colorPart * 3 + channel];
}

bool Settings::_setColor(uint8_t channel, int16_t x)
{
    if (x < kSliderLeft) x = kSliderLeft;
    if (x >= kSliderRight) x = kSliderRight - 1;
    const uint8_t value = static_cast<uint8_t>(
        (static_cast<uint32_t>(x - kSliderLeft) * 255) / (kSliderRight - kSliderLeft - 1));
    uint8_t& current = _colorChannel(channel);
    if (current == value) return false;
    current = value;
    return true;
}

void Settings::touchBegin(int16_t x, int16_t y)
{
    _pressed = _hit(x, y);
    _colorChanged = false;
    if (_page == 2 && _pressed >= 2 && _pressed <= 4)
    {
        _colorOriginal = _colorChannel(static_cast<uint8_t>(_pressed - 2));
        _colorChanged = _setColor(static_cast<uint8_t>(_pressed - 2), x);
    }
}

bool Settings::touchMove(int16_t x, int16_t y)
{
    if (_pressed < 0) return false;
    const int8_t target = _hit(x, y);
    if (_page == 2 && _pressed >= 2 && _pressed <= 4)
    {
        const uint8_t channel = static_cast<uint8_t>(_pressed - 2);
        if (target == _pressed)
        {
            const bool changed = _setColor(channel, x);
            _colorChanged |= changed;
            return changed;
        }
        _colorChannel(channel) = _colorOriginal;
        _colorChanged = false;
        _pressed = -1;
        return true;
    }
    if (target == _pressed) return false;
    _pressed = -1;
    return true;
}

void Settings::touchEnd(int16_t x, int16_t y)
{
    const int8_t released = _hit(x, y);
    if (_page == 2 && _pressed >= 2 && _pressed <= 4)
    {
        if (released == _pressed)
        {
            if (_colorChanged) _save();
        }
        else
        {
            _colorChannel(static_cast<uint8_t>(_pressed - 2)) = _colorOriginal;
        }
        _colorChanged = false;
        _pressed = -1;
        return;
    }
    if (_pressed >= 0 && released == _pressed) _activate(_pressed);
    _pressed = -1;
}

void Settings::_activate(int8_t target)
{
    if (target == 0) { close(); return; }
    if (target == 5) { _page = (_page + kPageCount - 1) % kPageCount; return; }
    if (target == 6) { _page = (_page + 1) % kPageCount; return; }
    if (_page == 0)
    {
        if (target == 1) _data.brightness = (_data.brightness % 5) + 1;
        else if (target == 2) _data.audio ^= 1;
        else if (target == 3) _data.theme = (_data.theme + 1) % kThemeCount;
        else if (target == 4) _data.ledBrightness = (_data.ledBrightness + 1) % 4;
    }
    else if (_page == 1)
    {
        if (target == 1) _data.botStyle = (_data.botStyle + 1) % 5;
        else if (target == 2) _data.expression = (_data.expression + 1) % 10;
        else if (target == 3) _data.animation = (_data.animation + 1) % 8;
        else if (target == 4) { _page = 2; return; }
    }
    else if (_page == 2)
    {
        if (target == 1) { _colorPart = (_colorPart + 1) % 3; return; }
    }
    else if (_page == 3)
    {
        if (target == 1) _data.motion = (_data.motion + 1) % 5;
        else if (target == 2) _data.reducedMotion ^= 1;
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
    cv.drawString(pageName(_page), 48, 20);
    cv.fillRoundRect(278, 4, 36, 32, 8, _pressed == 0 ? style.accentColor : rowBg);
    cv.setTextDatum(middle_center);
    const uint16_t closeColor = _pressed == 0 ? botux::rgb565(255, 255, 255) : fg;
    cv.setTextColor(closeColor);
    cv.drawLine(289, 13, 303, 27, closeColor);
    cv.drawLine(303, 13, 289, 27, closeColor);

    if (_page == 0)
    {
        char level[2] = {static_cast<char>('0' + _data.brightness), '\0'};
        drawRow(cv, kRowY[0], "DISPLAY", level, _pressed == 1, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[1], "AUDIO FEEDBACK", onOff(_data.audio), _pressed == 2, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[2], "THEME", themeName(_data.theme), _pressed == 3, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[3], "BOTTOM LEDs", ledName(_data.ledBrightness), _pressed == 4, rowBg, fg, style.accentColor);
    }
    else if (_page == 1)
    {
        drawRow(cv, kRowY[0], "STYLE", botStyleName(_data.botStyle), _pressed == 1, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[1], "EXPRESSION", expressionName(_data.expression), _pressed == 2, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[2], "MOTION", animationName(_data.animation), _pressed == 3, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[3], "COLOR", "EDIT", _pressed == 4, rowBg, fg, style.accentColor);
    }
    else if (_page == 2)
    {
        const uint8_t red = _colorPart == 0 ? _data.bodyRed : (_colorPart == 1 ? _data.eyeRed : _data.accentRed);
        const uint8_t green = _colorPart == 0 ? _data.bodyGreen : (_colorPart == 1 ? _data.eyeGreen : _data.accentGreen);
        const uint8_t blue = _colorPart == 0 ? _data.bodyBlue : (_colorPart == 1 ? _data.eyeBlue : _data.accentBlue);
        const uint16_t selectedColor = botux::rgb565(red, green, blue);
        drawRow(cv, kRowY[0], "COLOR PART", colorPartName(_colorPart), _pressed == 1,
                rowBg, fg, style.accentColor);
        cv.fillRoundRect(210, 49, 34, 22, 5, selectedColor);
        cv.drawRoundRect(210, 49, 34, 22, 5, kLine);
        drawSlider(cv, kRowY[1], "R", red, 0, red, green, blue, fg, kLine);
        drawSlider(cv, kRowY[2], "G", green, 1, red, green, blue, fg, kLine);
        drawSlider(cv, kRowY[3], "B", blue, 2, red, green, blue, fg, kLine);
    }
    else
    {
        drawRow(cv, kRowY[0], "IMU MOTION", motionName(_data.motion), _pressed == 1, rowBg, fg, style.accentColor);
        drawRow(cv, kRowY[1], "REDUCED MOTION", onOff(_data.reducedMotion), _pressed == 2, rowBg, fg, style.accentColor);
        cv.setTextDatum(middle_left);
        cv.setTextColor(fg);
        cv.drawString("TILT AND SHAKE RESPONSE", 16, 145);
    }

    drawPager(cv, _page, _pressed, rowBg, fg, style.accentColor);
}
