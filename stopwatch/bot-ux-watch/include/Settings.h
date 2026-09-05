// NVS-backed watch preferences and the matching bot/UI palettes.
#pragma once

#include <Preferences.h>
#include <BotUx.h>
#include <stdint.h>

class Settings {
public:
    static const uint8_t THEME_NIGHT  = 0;
    static const uint8_t THEME_DUSK   = 1;
    static const uint8_t THEME_MONO   = 2;
    static const uint8_t THEME_COUNT  = 3;
    static const uint8_t APPEARANCE_COUNT = 3;

    struct Data {
        bool    hour24      = true;
        bool    showSeconds = false;
        bool    sound       = true;
        uint8_t theme       = THEME_NIGHT;
        uint8_t appearance  = 0;
        uint8_t brightness  = 3;     // 1..5
    };

    void begin();        // load from NVS (defaults on first boot)
    void save();         // persist data() to NVS

    Data& data() { return _data; }

    void rebuildStyle(); // recompute _style from _data
    const botux::BotUx::Style& style() const { return _style; }
    void apply(botux::BotUx& bot) const { bot.setStyle(_style); }

    static const char* themeName(uint8_t idx);
    static const char* appearanceName(uint8_t idx);

    // UI text never borrows the bot eye color: official-style eyes are dark.
    uint16_t ink() const;
    uint16_t muted() const;
    uint16_t panel() const;
    uint16_t warning() const;

private:
    Data _data;
    botux::BotUx::Style _style;
};
