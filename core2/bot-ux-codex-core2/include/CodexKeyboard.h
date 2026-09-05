// CodexKeyboard — compact 3-row QWERTY + command-chip row for the Core2 codex app.
//
// Owns key geometry, the shift/symbol layer state, and key→char mapping. It renders
// into the caller's full-screen M5Canvas and never owns the display. main.cpp owns the
// prompt buffer and dispatches touch to hitTest().
#pragma once

#include <M5GFX.h> // M5Canvas (== LGFX_Sprite)
#include <stdint.h>

namespace botux { struct Style; } // fwd — colors come from the active theme

class CodexKeyboard {
public:
    enum Kind : uint8_t {
        K_CHAR = 0,   // insert a character (shift/layer aware)
        K_BACKSPACE,  // delete last prompt char
        K_ENTER,      // submit (same as SEND)
        K_SHIFT,      // toggle upper/lower case
        K_LAYER,      // toggle letter/symbol layer (the "123" key)
        K_SPACE,      // insert a space
        K_CHIP,       // insert a canned prompt ("what time"…)
        K_SEND,       // submit the prompt
    };

    static const int16_t kMaxKeys = 38;

    void begin(M5Canvas* cv, const botux::Style* style, int16_t topY);

    // Touch → key id (index into this keyboard), or -1.
    int16_t hitTest(int16_t x, int16_t y);
    bool    contains(int16_t y) const { return y >= _topY; }

    Kind        kind(int16_t id) const;
    char        keyChar(int16_t id) const;    // K_CHAR/K_SPACE only
    const char* keyText(int16_t id) const;    // K_CHIP only, else nullptr

    void toggleShift();
    void toggleLayer();
    bool symbolLayer() const { return _layer; }

    void setChipsOnly(bool on);               // settings: QWERTY vs chips-only
    bool chipsOnly() const { return _chipsOnly; }

    void draw(int16_t pressedKey);

private:
    struct Key {
        int16_t x, y, w, h;
        Kind kind;
        char lo;             // lowercase (letter layer)
        char sy;             // symbol layer
        const char* label;   // static label for special keys / chips
        const char* text;    // chip insert text (static)
    };

    M5Canvas* _cv = nullptr;
    const botux::Style* _style = nullptr;
    int16_t _topY = 0;
    bool _shift = false;
    bool _layer = false;     // true = symbol/number layer
    bool _chipsOnly = false;

    Key     _keys[kMaxKeys];
    int16_t _count = 0;

    void _layout();                       // (re)compute key rects from _topY/_chipsOnly
    const char* _label(const Key& k);     // render label (layer/shift aware)
};
