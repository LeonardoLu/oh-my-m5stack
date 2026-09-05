// Settings — NVS-backed preferences + the three theme presets.
//
// Owns the persisted user choices (12/24 h, theme, eye/body style, brightness)
// and the theme color tables (tmp/ux-design.md §A.7). `style()` is a cached
// botux::BotUx::Style rebuilt from the current choices so callers can bot.setStyle()
// live; save() persists to NVS via Preferences.
#pragma once

#include <Preferences.h>
#include <BotUx.h>
#include <stdint.h>

class Settings {
public:
    static const uint8_t THEME_TEAL   = 0;
    static const uint8_t THEME_SUNSET = 1;
    static const uint8_t THEME_MONO   = 2;
    static const uint8_t THEME_COUNT  = 3;

    struct Data {
        bool    hour24     = true;   // false = 12 h
        uint8_t theme      = THEME_TEAL;
        uint8_t eyeStyle   = 0;      // botux::BotUx::EyeStyle::Round
        uint8_t bodyStyle  = 1;      // botux::BotUx::BodyStyle::Round
        uint8_t brightness = 3;      // 1..5
    };

    void begin();        // load from NVS (defaults on first boot)
    void save();         // persist data() to NVS

    Data& data() { return _data; }

    void rebuildStyle(); // recompute _style from _data
    const botux::BotUx::Style& style() const { return _style; }
    void apply(botux::BotUx& bot) const { bot.setStyle(_style); }

    static const char* themeName(uint8_t idx);

private:
    Data _data;
    botux::BotUx::Style _style;
};
