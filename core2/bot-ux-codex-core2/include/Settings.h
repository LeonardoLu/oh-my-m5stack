// Settings — NVS-backed preferences + settings screen for the Core2 codex app.
//
// Persists keyboard layout, theme, haptics, and brightness with Preferences. Renders
// its own screen (list + theme carousel) and interprets touch while open. main.cpp
// forwards touch to it when it is open and applies its values to the bot/keyboard.
#pragma once

#include <M5GFX.h> // M5Canvas
#include <stdint.h>

#include "BotUx.h" // botux::Style / BotUx (returned by value)

class Settings {
public:
    struct Data {
        uint8_t kbdLayout;   // 0 = QWERTY, 1 = chips-only
        uint8_t theme;       // 0..2
        uint8_t haptics;     // 0/1
        uint8_t brightness;  // 1..5
    };

    enum { kThemeCount = 3 };

    static const char* themeName(int i);
    static botux::Style themeStyle(int i);

    void begin();                    // load prefs from NVS
    void apply(botux::BotUx* bot);   // push theme + brightness to device
    void applyBrightness();

    const Data& data() const { return _data; }
    void cycleTheme(int dir);        // wrap + save (bot swipe L/R + carousel)

    bool isOpen() const { return _open; }
    void open();
    void close();

    // touch dispatch — only called while open
    void touchBegin(int16_t x, int16_t y);
    void touchMove(int16_t x, int16_t y);
    void touchEnd(int16_t x, int16_t y);

    void draw(M5Canvas* cv, M5Canvas* botSprite); // botSprite = live preview

private:
    enum Sub { List, Carousel };

    int16_t _rowAt(int16_t x, int16_t y) const;
    void _setBrightnessFromX(int16_t x);
    void _handleRow(int16_t row);
    void _save();

    Data _data = {0, 0, 1, 3};
    bool _open = false;
    Sub  _sub = List;

    // gesture state (carousel swipe + brightness drag)
    int16_t _px = 0, _py = 0;
    bool _down = false;
    bool _swiped = false;
    int16_t _dragRow = -1;   // -1 = none
};
