#include "Settings.h"

namespace {
struct ThemeColors {
    uint16_t bg, body, accent, eye, pupil, mouth, blush;
};

// RGB565 color tables — tmp/ux-design.md §A.7.
const ThemeColors kThemes[Settings::THEME_COUNT] = {
    // Grok Teal (default)
    { botux::rgb565(0x0A,0x0E,0x14), botux::rgb565(0x2A,0x2E,0x38), botux::rgb565(0x2E,0xD9,0xC8),
      botux::rgb565(0xFF,0xFF,0xFF), botux::rgb565(0x0A,0x12,0x22), botux::rgb565(0xFF,0xFF,0xFF),
      botux::rgb565(0xFF,0x8A,0xA0) },
    // Sunset
    { botux::rgb565(0x1A,0x0F,0x14), botux::rgb565(0x4A,0x2A,0x30), botux::rgb565(0xFF,0x9F,0x43),
      botux::rgb565(0xFF,0xE8,0xC8), botux::rgb565(0x2A,0x15,0x20), botux::rgb565(0xFF,0xD9,0xA0),
      botux::rgb565(0xFF,0x6B,0x81) },
    // Mono
    { botux::rgb565(0x0F,0x0F,0x0F), botux::rgb565(0x2E,0x2E,0x2E), botux::rgb565(0xE6,0xE6,0xE6),
      botux::rgb565(0xFF,0xFF,0xFF), botux::rgb565(0x11,0x11,0x11), botux::rgb565(0xFF,0xFF,0xFF),
      botux::rgb565(0x77,0x77,0x77) },
};

const char* kThemeNames[Settings::THEME_COUNT] = { "Grok Teal", "Sunset", "Mono" };
const char* kNamespace = "watch";
} // namespace

void Settings::begin() {
    Preferences prefs;
    prefs.begin(kNamespace, false);
    _data.hour24     = prefs.getBool("hour24", _data.hour24);
    _data.theme      = prefs.getUChar("theme", _data.theme);
    _data.eyeStyle   = prefs.getUChar("eye", _data.eyeStyle);
    _data.bodyStyle  = prefs.getUChar("body", _data.bodyStyle);
    _data.brightness = prefs.getUChar("bright", _data.brightness);
    prefs.end();

    // guard against a corrupt NVS
    if (_data.theme >= THEME_COUNT) _data.theme = THEME_TEAL;
    if (_data.eyeStyle > 3) _data.eyeStyle = 0;
    if (_data.bodyStyle > 3) _data.bodyStyle = 1;
    if (_data.brightness < 1 || _data.brightness > 5) _data.brightness = 3;

    rebuildStyle();
}

void Settings::save() {
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.putBool("hour24", _data.hour24);
    prefs.putUChar("theme", _data.theme);
    prefs.putUChar("eye", _data.eyeStyle);
    prefs.putUChar("body", _data.bodyStyle);
    prefs.putUChar("bright", _data.brightness);
    prefs.end();
}

void Settings::rebuildStyle() {
    const ThemeColors& c = kThemes[_data.theme];
    botux::Style s;                 // Grok Teal defaults
    s.bgColor     = c.bg;
    s.bodyColor   = c.body;
    s.accentColor = c.accent;
    s.eyeColor    = c.eye;
    s.pupilColor  = c.pupil;
    s.mouthColor  = c.mouth;
    s.blushColor  = c.blush;
    s.eyeStyle    = (botux::BotUx::EyeStyle)_data.eyeStyle;
    s.bodyStyle   = (botux::BotUx::BodyStyle)_data.bodyStyle;
    _style = s;
}

const char* Settings::themeName(uint8_t idx) {
    return (idx < THEME_COUNT) ? kThemeNames[idx] : "?";
}
