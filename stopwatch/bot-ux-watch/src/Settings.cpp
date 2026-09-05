#include "Settings.h"

namespace {
struct ThemeColors {
    uint16_t bg, body, accent, eye, pupil, mouth, blush;
    uint16_t ink, muted, panel, warning;
};

const ThemeColors kThemes[Settings::THEME_COUNT] = {
    { botux::rgb565(0x08,0x0B,0x10), botux::rgb565(0xF2,0xF0,0xE8), botux::rgb565(0x55,0x8D,0xFF),
      botux::rgb565(0x19,0x22,0x32), botux::rgb565(0x19,0x22,0x32), botux::rgb565(0x19,0x22,0x32),
      botux::rgb565(0xFA,0x9A,0xA8), botux::rgb565(0xF7,0xF8,0xFA), botux::rgb565(0x98,0xA2,0xB3),
      botux::rgb565(0x18,0x20,0x2D), botux::rgb565(0xFF,0x5F,0x68) },
    { botux::rgb565(0x1A,0x10,0x17), botux::rgb565(0xF5,0xEC,0xDE), botux::rgb565(0xFF,0x9B,0x62),
      botux::rgb565(0x2A,0x1D,0x27), botux::rgb565(0x2A,0x1D,0x27), botux::rgb565(0x2A,0x1D,0x27),
      botux::rgb565(0xFA,0x95,0xA0), botux::rgb565(0xFF,0xF7,0xED), botux::rgb565(0xB8,0x9E,0xA9),
      botux::rgb565(0x35,0x20,0x2B), botux::rgb565(0xFF,0x68,0x68) },
    { botux::rgb565(0x0C,0x0D,0x10), botux::rgb565(0xEC,0xEC,0xE8), botux::rgb565(0xC7,0xCF,0xDC),
      botux::rgb565(0x19,0x1B,0x20), botux::rgb565(0x19,0x1B,0x20), botux::rgb565(0x19,0x1B,0x20),
      botux::rgb565(0x9A,0x9A,0x9A), botux::rgb565(0xF4,0xF4,0xF2), botux::rgb565(0x9B,0x9E,0xA5),
      botux::rgb565(0x25,0x27,0x2C), botux::rgb565(0xFF,0x62,0x62) },
};

const char* kThemeNames[Settings::THEME_COUNT] = { "Night", "Dusk", "Mono" };
const char* kAppearanceNames[Settings::APPEARANCE_COUNT] = { "Orb", "Bean", "Pebble" };
const char* kExpressionNames[Settings::EXPRESSION_COUNT] = {
    "Auto", "Neutral", "Curious", "Focused", "Joy",
    "Skeptical", "Bashful", "Wink", "Dizzy", "Alarmed"
};
const char* kAnimationNames[Settings::ANIMATION_COUNT] = {
    "Auto", "Calm", "Curious", "Orbit", "Bounce", "Glitch", "Wave", "Sparkle"
};
const char* kNamespace = "watch";
} // namespace

void Settings::begin() {
    Preferences prefs;
    prefs.begin(kNamespace, false);
    _data.hour24      = prefs.getBool("hour24", _data.hour24);
    _data.showSeconds = prefs.getBool("seconds", _data.showSeconds);
    _data.sound       = prefs.getBool("sound", _data.sound);
    _data.theme       = prefs.getUChar("theme", _data.theme);
    _data.appearance  = prefs.getUChar("look", _data.appearance);
    _data.expression  = prefs.getUChar("expr", _data.expression);
    _data.animation   = prefs.getUChar("anim", _data.animation);
    _data.brightness  = prefs.getUChar("bright", _data.brightness);
    _data.motion      = prefs.getBool("motion", _data.motion);
    prefs.end();

    if (_data.theme >= THEME_COUNT) _data.theme = THEME_NIGHT;
    if (_data.appearance >= APPEARANCE_COUNT) _data.appearance = 0;
    if (_data.expression >= EXPRESSION_COUNT) _data.expression = 0;
    if (_data.animation >= ANIMATION_COUNT) _data.animation = 2;
    if (_data.brightness < 1 || _data.brightness > 5) _data.brightness = 3;

    rebuildStyle();
}

void Settings::save() {
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.putBool("hour24", _data.hour24);
    prefs.putBool("seconds", _data.showSeconds);
    prefs.putBool("sound", _data.sound);
    prefs.putUChar("theme", _data.theme);
    prefs.putUChar("look", _data.appearance);
    prefs.putUChar("expr", _data.expression);
    prefs.putUChar("anim", _data.animation);
    prefs.putUChar("bright", _data.brightness);
    prefs.putBool("motion", _data.motion);
    prefs.end();
}

void Settings::rebuildStyle() {
    const ThemeColors& c = kThemes[_data.theme];
    botux::BotUx::Style s;
    s.bgColor     = c.bg;
    s.bodyColor   = c.body;
    s.accentColor = c.accent;
    s.eyeColor    = c.eye;
    s.pupilColor  = c.pupil;
    s.mouthColor  = c.mouth;
    s.blushColor  = c.blush;
    s.eyeStyle = botux::BotUx::EyeStyle::Oval;
    static const botux::BotUx::BodyStyle bodies[APPEARANCE_COUNT] = {
        botux::BotUx::BodyStyle::Round,
        botux::BotUx::BodyStyle::RoundedSquare,
        botux::BotUx::BodyStyle::Hexagon,
    };
    s.bodyStyle = bodies[_data.appearance];
    _style = s;
}

const char* Settings::themeName(uint8_t idx) {
    return (idx < THEME_COUNT) ? kThemeNames[idx] : "?";
}

const char* Settings::appearanceName(uint8_t idx) {
    return (idx < APPEARANCE_COUNT) ? kAppearanceNames[idx] : "?";
}

const char* Settings::expressionName(uint8_t idx) {
    return (idx < EXPRESSION_COUNT) ? kExpressionNames[idx] : "?";
}

const char* Settings::animationName(uint8_t idx) {
    return (idx < ANIMATION_COUNT) ? kAnimationNames[idx] : "?";
}

uint16_t Settings::ink() const     { return kThemes[_data.theme].ink; }
uint16_t Settings::muted() const   { return kThemes[_data.theme].muted; }
uint16_t Settings::panel() const   { return kThemes[_data.theme].panel; }
uint16_t Settings::warning() const { return kThemes[_data.theme].warning; }
