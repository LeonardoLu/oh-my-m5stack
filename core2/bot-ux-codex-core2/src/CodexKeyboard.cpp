// CodexKeyboard implementation. See CodexKeyboard.h for the contract.
#include "CodexKeyboard.h"

namespace {

using K = CodexKeyboard;

struct RowDef {
    K::Kind kind;
    char lo;            // lowercase (letter layer)
    char sy;            // symbol layer
    const char* label;  // static label for special keys / chips
    const char* text;   // chip insert text (static)
};

// 38-key QWERTY layout, rows top→bottom. Row sizes: 11 / 10 / 11 / 6.
const RowDef kQwerty[K::kMaxKeys] = {
    // row 0: q w e r t y u i o p ⌫
    {K::K_CHAR, 'q', '1', nullptr, nullptr},
    {K::K_CHAR, 'w', '2', nullptr, nullptr},
    {K::K_CHAR, 'e', '3', nullptr, nullptr},
    {K::K_CHAR, 'r', '4', nullptr, nullptr},
    {K::K_CHAR, 't', '5', nullptr, nullptr},
    {K::K_CHAR, 'y', '6', nullptr, nullptr},
    {K::K_CHAR, 'u', '7', nullptr, nullptr},
    {K::K_CHAR, 'i', '8', nullptr, nullptr},
    {K::K_CHAR, 'o', '9', nullptr, nullptr},
    {K::K_CHAR, 'p', '0', nullptr, nullptr},
    {K::K_BACKSPACE, 0, 0, "DEL", nullptr},
    // row 1: a s d f g h j k l ⏎
    {K::K_CHAR, 'a', '-', nullptr, nullptr},
    {K::K_CHAR, 's', '_', nullptr, nullptr},
    {K::K_CHAR, 'd', '/', nullptr, nullptr},
    {K::K_CHAR, 'f', '?', nullptr, nullptr},
    {K::K_CHAR, 'g', '!', nullptr, nullptr},
    {K::K_CHAR, 'h', '.', nullptr, nullptr},
    {K::K_CHAR, 'j', ',', nullptr, nullptr},
    {K::K_CHAR, 'k', ':', nullptr, nullptr},
    {K::K_CHAR, 'l', ';', nullptr, nullptr},
    {K::K_ENTER, 0, 0, "GO", nullptr},
    // row 2: ⇧ z x c v b n m , . ⇧
    {K::K_SHIFT, 0, 0, "SH", nullptr},
    {K::K_CHAR, 'z', '(', nullptr, nullptr},
    {K::K_CHAR, 'x', ')', nullptr, nullptr},
    {K::K_CHAR, 'c', '\'', nullptr, nullptr},
    {K::K_CHAR, 'v', '"', nullptr, nullptr},
    {K::K_CHAR, 'b', '$', nullptr, nullptr},
    {K::K_CHAR, 'n', '#', nullptr, nullptr},
    {K::K_CHAR, 'm', '%', nullptr, nullptr},
    {K::K_CHAR, ',', ',', nullptr, nullptr},
    {K::K_CHAR, '.', '.', nullptr, nullptr},
    {K::K_SHIFT, 0, 0, "SH", nullptr},
    // row 3: 123 · space · what time · joke · hello · SEND
    {K::K_LAYER, 0, 0, "123", nullptr},
    {K::K_SPACE, 0, 0, "SPACE", nullptr},
    {K::K_CHIP, 0, 0, "what time", "what time"},
    {K::K_CHIP, 0, 0, "joke", "joke"},
    {K::K_CHIP, 0, 0, "hello", "hello"},
    {K::K_SEND, 0, 0, "SEND", nullptr},
};

// chips-only layout: a 2×2 grid of big command chips + SEND.
const RowDef kChips[4] = {
    {K::K_CHIP, 0, 0, "what time", "what time"},
    {K::K_CHIP, 0, 0, "joke", "joke"},
    {K::K_CHIP, 0, 0, "hello", "hello"},
    {K::K_SEND, 0, 0, "SEND", nullptr},
};

// ASCII uppercase, no <ctype.h> dependency.
char upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

} // namespace

void CodexKeyboard::begin(M5Canvas* cv, const botux::BotUx::Style* style, int16_t topY)
{
    _cv = cv;
    _style = style;
    _topY = topY;
    _layout();
}

void CodexKeyboard::_layout()
{
    if (_chipsOnly)
    {
        const int16_t cw = 156, ch = 52, gap = 8;
        for (int i = 0; i < 4; i++)
        {
            Key& k = _keys[i];
            k.kind = kChips[i].kind;
            k.lo = 0; k.sy = 0; k.label = kChips[i].label; k.text = kChips[i].text;
            k.x = (i % 2) * (cw + gap);
            k.y = _topY + (i / 2) * (ch + gap);
            k.w = cw; k.h = ch;
        }
        _count = 4;
        return;
    }

    int16_t W = (int16_t)_cv->width();
    static const int16_t row3W[6] = {44, 72, 64, 40, 40, 37};

    for (int i = 0; i < kMaxKeys; i++)
    {
        Key& k = _keys[i];
        const RowDef& d = kQwerty[i];
        k.kind = d.kind; k.lo = d.lo; k.sy = d.sy; k.label = d.label; k.text = d.text;

        int16_t row = (i < 11) ? 0 : (i < 21 ? 1 : (i < 32 ? 2 : 3));
        int16_t col = (row == 0) ? i : (row == 1 ? i - 11 : (row == 2 ? i - 21 : i - 32));

        if (row < 3)
        {
            int16_t n = (row == 1) ? 10 : 11;
            int16_t startX = (W - (n * 26 + (n - 1) * 3)) / 2;
            k.x = startX + col * 29;
            k.y = _topY + row * 28 + 1;
            k.w = 26; k.h = 26;
        }
        else
        {
            int16_t x = 4;
            for (int c = 0; c < col; c++) x += row3W[c] + 3;
            k.x = x;
            k.w = row3W[col];
            k.y = _topY + 3 * 28 + 1;
            k.h = 26;
        }
    }
    _count = kMaxKeys;
}

int16_t CodexKeyboard::hitTest(int16_t x, int16_t y)
{
    for (int16_t i = 0; i < _count; i++)
    {
        const Key& k = _keys[i];
        if (x >= k.x && x < k.x + k.w && y >= k.y && y < k.y + k.h) return i;
    }
    return -1;
}

CodexKeyboard::Kind CodexKeyboard::kind(int16_t id) const
{
    return (id >= 0 && id < _count) ? _keys[id].kind : K_CHAR;
}

char CodexKeyboard::keyChar(int16_t id) const
{
    if (id < 0 || id >= _count) return 0;
    const Key& k = _keys[id];
    if (k.kind == K_CHAR) return _layer ? k.sy : (_shift ? upper(k.lo) : k.lo);
    if (k.kind == K_SPACE) return ' ';
    return 0;
}

const char* CodexKeyboard::keyText(int16_t id) const
{
    if (id < 0 || id >= _count) return nullptr;
    return (_keys[id].kind == K_CHIP) ? _keys[id].text : nullptr;
}

void CodexKeyboard::toggleShift() { _shift = !_shift; }
void CodexKeyboard::toggleLayer() { _layer = !_layer; }

void CodexKeyboard::setChipsOnly(bool on)
{
    _chipsOnly = on;
    _layout();
}

const char* CodexKeyboard::_label(const Key& k)
{
    static char buf[2];
    switch (k.kind)
    {
        case K_CHAR:
            buf[0] = _layer ? k.sy : (_shift ? upper(k.lo) : k.lo);
            buf[1] = 0;
            return buf;
        case K_LAYER:
            return _layer ? "abc" : "123";
        default:
            return k.label;
    }
}

void CodexKeyboard::draw(int16_t pressedKey)
{
    for (int16_t i = 0; i < _count; i++)
    {
        const Key& k = _keys[i];
        bool on = (i == pressedKey);
        uint16_t bg = on ? _style->accentColor : _style->bodyColor;
        uint16_t fg = on ? _style->bgColor : _style->eyeColor;

        _cv->fillRoundRect(k.x, k.y, k.w, k.h, 5, bg);
        if (on) _cv->drawRoundRect(k.x, k.y, k.w, k.h, 5, _style->eyeColor);

        _cv->setTextDatum(middle_center);
        _cv->setTextColor(fg);
        _cv->setTextSize((k.kind == K_CHAR) ? 2.0f : 1.0f);
        _cv->drawString(_label(k), k.x + k.w / 2, k.y + k.h / 2);
    }
}
