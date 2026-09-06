#pragma once

#include <M5GFX.h>
#include <stdint.h>

#include "BotUx.h"

class Settings {
public:
    struct Data {
        uint8_t theme;
        uint8_t audio;
        uint8_t brightness;
        uint8_t reducedMotion;
        uint8_t ledBrightness;
        uint8_t animation;
        uint8_t motion;
        uint8_t botStyle;
        uint8_t expression;
        uint8_t bodyRed;
        uint8_t bodyGreen;
        uint8_t bodyBlue;
        uint8_t eyeRed;
        uint8_t eyeGreen;
        uint8_t eyeBlue;
        uint8_t accentRed;
        uint8_t accentGreen;
        uint8_t accentBlue;
    };

    static constexpr uint8_t kThemeCount = 3;
    static constexpr uint8_t kPageCount = 4;

    void begin();
    void apply(botux::BotUx& bot) const;
    void applyBrightness() const;
    const Data& data() const { return _data; }
    botux::BotUx::Style botStyle() const;

    bool isOpen() const { return _open; }
    void open(uint8_t page = 0);
    void close();
    void touchBegin(int16_t x, int16_t y);
    bool touchMove(int16_t x, int16_t y);
    void touchEnd(int16_t x, int16_t y);
    void draw(M5Canvas& cv, M5Canvas& botSprite) const;

    static const char* themeName(uint8_t value);
    static botux::BotUx::Style themeStyle(uint8_t value);

private:
    int8_t _hit(int16_t x, int16_t y) const;
    uint8_t& _colorChannel(uint8_t channel);
    bool _setColor(uint8_t channel, int16_t x);
    void _activate(int8_t target);
    void _save() const;

    Data _data{0, 1, 3, 0, 2, 0, 2, 0, 0,
               252, 252, 250, 33, 36, 42, 60, 124, 232};
    bool _open = false;
    uint8_t _page = 0;
    uint8_t _colorPart = 0;
    uint8_t _colorOriginal = 0;
    bool _colorChanged = false;
    int8_t _pressed = -1;
};
