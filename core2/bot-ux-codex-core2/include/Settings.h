#pragma once

#include <M5GFX.h>
#include <stdint.h>

#include "BotUx.h"

class Settings {
public:
    struct Data {
        uint8_t speed;
        uint8_t theme;
        uint8_t audio;
        uint8_t brightness;
        uint8_t reducedMotion;
        uint8_t ledBrightness;
    };

    static constexpr uint8_t kThemeCount = 3;

    void begin();
    void apply(botux::BotUx& bot) const;
    void applyBrightness() const;
    const Data& data() const { return _data; }

    bool isOpen() const { return _open; }
    void open();
    void close();
    void touchBegin(int16_t x, int16_t y);
    void touchMove(int16_t x, int16_t y);
    void touchEnd(int16_t x, int16_t y);
    void draw(M5Canvas& cv, M5Canvas& botSprite) const;

    static const char* themeName(uint8_t value);
    static botux::BotUx::Style themeStyle(uint8_t value);

private:
    int8_t _hit(int16_t x, int16_t y) const;
    void _activate(int8_t target);
    void _save() const;

    Data _data{1, 0, 1, 3, 0, 2};
    bool _open = false;
    uint8_t _page = 0;
    int8_t _pressed = -1;
};
