// BotUx — animation implementation. See BotUx.h for the contract.
#include "BotUx.h"

#include <Arduino.h> // millis()
#include <math.h>
#include <stdio.h>

namespace botux {

namespace {
constexpr float kPi = 3.14159265358979f;
// blink phase durations (ms)
constexpr uint32_t BLINK_CLOSE_MS = 80;
constexpr uint32_t BLINK_HOLD_MS  = 60; // extra hold while fully closed
constexpr uint32_t BLINK_OPEN_MS  = 80;
// poke() reaction windows
constexpr uint32_t SURPRISE_MS = 600;
constexpr uint32_t HAPPY_MS     = 1800;

// Layout: everything is derived from the canvas size and the eye-size scale so
// the same component fills a watch (135x240) or a tile (any size) gracefully.
BotUx::Metrics computeLayout(int16_t w, int16_t h, const BotUx::Style& s) {
    BotUx::Metrics m;
    int16_t side = (w < h) ? w : h;
    m.bodyR = (int16_t)(side * 0.36f);
    m.eyeRadius = (int16_t)(side * 0.105f * s.eyeSize);
    if (m.eyeRadius < 3) m.eyeRadius = 3;
    m.eyeDX = (int16_t)(m.eyeRadius * 1.45f);
    m.cx = w / 2;
    m.cy = (int16_t)(h * 0.40f);
    m.eyeY = m.cy - (int16_t)(m.eyeRadius * 0.55f);
    m.mouthY = m.cy + (int16_t)(m.eyeRadius * 1.55f);
    return m;
}
} // namespace

void BotUx::begin(M5Canvas* canvas) {
    _cv = canvas;
    _w = (int16_t)canvas->width();
    _h = (int16_t)canvas->height();
    _m = computeLayout(_w, _h, _style);
    _animStart = millis();
    uint32_t span = _style.blinkMaxMs - _style.blinkMinMs;
    _nextBlink = _animStart + _style.blinkMinMs + (_blinkSeed % (span + 1));
}

void BotUx::setStyle(const Style& s) {
    _style = s;
    _m = computeLayout(_w, _h, _style); // eye scale may move things around
}

void BotUx::setMood(Mood m) { _mood = m; }

void BotUx::setTalking(bool on) {
    if (on && !_talking) _talkSeed = millis();
    _talking = on;
}

void BotUx::setBattery(uint8_t pct) {
    _battery = (pct > 100) ? 100 : pct;
    _batteryVisible = true;
}

void BotUx::setBatteryVisible(bool vis) { _batteryVisible = vis; }

void BotUx::setSignal(int8_t bars) {
    if (bars < -1) bars = -1;
    if (bars > 4) bars = 4;
    _signal = bars;
}

void BotUx::setTime(uint8_t h, uint8_t m, uint8_t s, bool pm) {
    _hasTime = true;
    _hh = h % 12; if (_hh == 0) _hh = 12;
    _mm = m % 60;
    _ss = s % 60;
    _pm = pm;
}

void BotUx::setLabel(const char* text) { _label = text; }

void BotUx::poke() {
    uint32_t now = millis();
    _pokeUntil = now + SURPRISE_MS;
    _reactionUntil = now + HAPPY_MS;
}

float BotUx::_pulse(uint32_t ms, float period, float phase) const {
    return 0.5f + 0.5f * sinf(2.0f * kPi * ms / period + phase);
}

void BotUx::update(uint32_t nowMs) {
    _now = nowMs ? nowMs : millis();
    if (!_cv) return;
    _updateBlink(_now);
    _updatePupil(_now);
    _updateTalk(_now);
    _resolveMood(_now);
    // ~2.6s breathing cycle
    _breath = sinf(2.0f * kPi * (_now - _animStart) / 3800.0f);
    _lastNow = _now;
}

void BotUx::_updateBlink(uint32_t now) {
    if (!_blinking) {
        if ((int32_t)(now - _nextBlink) >= 0) {
            _blinking = true;
            _blinkStart = now;
            _blinkPhase = 0;
        }
        return;
    }
    uint32_t t = now - _blinkStart;
    if (_blinkPhase == 0 && t >= BLINK_CLOSE_MS) _blinkPhase = 1;
    else if (_blinkPhase == 1 && t >= BLINK_CLOSE_MS + BLINK_HOLD_MS) _blinkPhase = 2;
    else if (_blinkPhase == 2 && t >= BLINK_CLOSE_MS + BLINK_HOLD_MS + BLINK_OPEN_MS) {
        _blinking = false;
        uint32_t span = _style.blinkMaxMs - _style.blinkMinMs;
        _nextBlink = now + _style.blinkMinMs + (_blinkSeed % (span + 1));
        _blinkSeed = _blinkSeed * 1103515245u + 12345u; // LCG — deterministic shuffle
    }
}

void BotUx::_updatePupil(uint32_t now) {
    uint32_t t = now - _animStart;
    // Two incommensurate sines give smooth, non-repetitive-looking drift.
    _pupilDX = 0.55f * sinf(2.0f * kPi * t / 3400.0f) + 0.35f * sinf(2.0f * kPi * t / 1700.0f + 1.7f);
    _pupilDY = 0.45f * sinf(2.0f * kPi * t / 2900.0f + 0.6f) + 0.30f * sinf(2.0f * kPi * t / 4100.0f + 3.1f);
}

void BotUx::_updateTalk(uint32_t now) {
    if (!_talking) {
        _talkAmp *= 0.88f; // decay so the mouth eases shut
        if (_talkAmp < 0.02f) _talkAmp = 0.0f;
        return;
    }
    uint32_t t = now - _talkSeed;
    // Fast flap modulated by a slower syllable envelope.
    float flap = _pulse(t, 130.0f);
    float env = _pulse(t, 310.0f, 1.2f);
    _talkAmp = 0.25f + 0.75f * flap * env;
}

void BotUx::_resolveMood(uint32_t now) {
    if (now < _pokeUntil) _effMood = Mood::Surprised;
    else if (now < _reactionUntil) _effMood = Mood::Happy;
    else _effMood = _mood;

    float target = 1.0f;
    switch (_effMood) {
        case Mood::Sleepy:     target = 0.25f; break;
        case Mood::Sad:        target = 0.45f; break;
        case Mood::Thinking:   target = 0.90f; break;
        case Mood::Listening:  target = 1.15f; break;
        case Mood::Surprised:  target = 1.25f; break;
        default:               target = 1.00f; break;
    }
    if (_battery <= 10 && target > 0.30f) target = 0.30f; // sleepy when low

    // Ease the base openness toward the target so mood changes never snap
    // (framerate-independent exponential smoothing, ~180 ms time constant).
    uint32_t dt = (_lastNow == 0) ? 16u : (now - _lastNow);
    if (dt > 100u) dt = 100u; // clamp long gaps (sleep/resume)
    float a = (float)dt / ((float)dt + 180.0f);
    _openBase += (target - _openBase) * a;

    float open = _openBase;
    if (_blinking) {
        uint32_t t = now - _blinkStart;
        float p;
        if (_blinkPhase == 0)      p = (float)t / BLINK_CLOSE_MS;
        else if (_blinkPhase == 1) p = 1.0f;
        else                       p = 1.0f - (float)(t - BLINK_CLOSE_MS - BLINK_HOLD_MS) / BLINK_OPEN_MS;
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
        open *= (1.0f - _smooth(p));
    }
    _open = open;
}

void BotUx::draw() {
    if (!_cv) return;
    _cv->fillSprite(_style.bgColor);
    _drawBody();
    _drawEyes();
    _drawBrows();
    _drawMouth();
    _drawAntenna();
    _drawOverlays();
}

void BotUx::_drawBody() {
    if (_style.bodyStyle == BodyStyle::None) return;
    int16_t cx = _m.cx, cy = _m.cy;
    int16_t r = (int16_t)(_m.bodyR * (1.0f + _breath * 0.04f));
    uint16_t body = _style.bodyColor, edge = _style.accentColor;

    switch (_style.bodyStyle) {
        case BodyStyle::Round:
            _cv->fillCircle(cx, cy, r, body);
            _cv->drawCircle(cx, cy, r, edge);
            break;
        case BodyStyle::RoundedSquare:
            _cv->fillRoundRect(cx - r, cy - r, r * 2, r * 2, r / 3, body);
            _cv->drawRoundRect(cx - r, cy - r, r * 2, r * 2, r / 3, edge);
            break;
        case BodyStyle::Hexagon: {
            int32_t px[6], py[6];
            for (int i = 0; i < 6; i++) {
                float a = kPi / 6.0f + i * kPi / 3.0f; // flat-top hexagon
                px[i] = cx + (int32_t)(r * cosf(a));
                py[i] = cy + (int32_t)(r * sinf(a));
            }
            for (int i = 0; i < 6; i++)
                _cv->fillTriangle(cx, cy, px[i], py[i], px[(i + 1) % 6], py[(i + 1) % 6], body);
            for (int i = 0; i < 6; i++)
                _cv->drawLine(px[i], py[i], px[(i + 1) % 6], py[(i + 1) % 6], edge);
            break;
        }
        default: break;
    }
}

void BotUx::_drawEyes() {
    int16_t cx = _m.cx, eyeY = _m.eyeY, dx = _m.eyeDX;
    float er = (float)_m.eyeRadius;

    // Happy: ^ ^ closed arcs, no eyeballs.
    if (_effMood == Mood::Happy) {
        for (int s = -1; s <= 1; s += 2) {
            int16_t ex = cx + s * dx;
            // LovyanGFX arc convention: 0° = right, clockwise (90° = down, 270° = up).
            // 180°..360° sweeps left→top→right = the top arc (a "∩" happy eye).
            _cv->fillArc(ex, eyeY, (int16_t)(er * 0.45f), (int16_t)(er * 1.05f), 180, 360, _style.eyeColor);
        }
        return;
    }

    float open = _open;
    float eh = er * open;
    if (eh < 0.5f) eh = 0.0f;

    for (int s = -1; s <= 1; s += 2) {
        int16_t ex = cx + s * dx;

        // Pupil target by mood.
        float ppx = _pupilDX * er * 0.35f;
        float ppy = _pupilDY * er * 0.35f;
        if (_effMood == Mood::Thinking)     { ppx = s * er * 0.15f; ppy = -er * 0.45f; }
        else if (_effMood == Mood::Sad)     { ppx = s * er * 0.10f; ppy =  er * 0.35f; }
        else if (_effMood == Mood::Listening || _effMood == Mood::Surprised) { ppx = 0; ppy = 0; }

        switch (_style.eyeStyle) {
            case EyeStyle::Round:
                _cv->fillEllipse(ex, eyeY, (int16_t)er, (int16_t)eh, _style.eyeColor);
                break;
            case EyeStyle::Oval:
                _cv->fillEllipse(ex, eyeY, (int16_t)(er * 0.85f), (int16_t)(eh * 1.25f), _style.eyeColor);
                break;
            case EyeStyle::Square: {
                int16_t hh = (int16_t)eh;
                _cv->fillRoundRect(ex - (int16_t)er, eyeY - hh, (int16_t)(er * 2.0f), hh * 2, (int16_t)(er * 0.5f), _style.eyeColor);
                break;
            }
            case EyeStyle::Googly:
                _cv->fillEllipse(ex, eyeY, (int16_t)er, (int16_t)eh, _style.eyeColor);
                break;
        }

        if (eh > er * 0.30f) {
            float pr = er * ((_effMood == Mood::Surprised) ? 0.25f : 0.42f);
            _cv->fillCircle(ex + (int16_t)ppx, eyeY + (int16_t)ppy, (int16_t)pr, _style.pupilColor);
        }
    }
}

void BotUx::_drawBrows() {
    Mood e = _effMood;
    if (e == Mood::Happy || e == Mood::Idle || e == Mood::Speaking || e == Mood::Sleepy) return;

    int16_t er = _m.eyeRadius, cx = _m.cx, dx = _m.eyeDX;
    int16_t bw = (int16_t)(er * 1.4f);
    int16_t th = (int16_t)(er * 0.20f);
    if (th < 2) th = 2;
    int16_t baseY = _m.eyeY - (int16_t)(er * 1.7f);
    int16_t lift = 0, innerLift = 0;
    switch (e) {
        case Mood::Listening: lift = (int16_t)(er * 0.30f); break;
        case Mood::Thinking:  innerLift = -(int16_t)(er * 0.35f); lift = (int16_t)(er * 0.10f); break; // knit: inner ends down
        case Mood::Sad:       innerLift = (int16_t)(er * 0.40f); break;                                // worried: inner ends up
        case Mood::Surprised: lift = (int16_t)(er * 0.75f); break;
        default: break;
    }

    for (int s = -1; s <= 1; s += 2) {
        int16_t ex = cx + s * dx;
        int16_t ox = ex + s * bw; // outer end (away from centre)
        int16_t ix = ex - s * bw; // inner end (toward centre)
        int16_t oy = baseY - lift;
        int16_t iy = baseY - lift - innerLift;
        // A tapered sliver (two triangles) reads as a slanted brow.
        _cv->fillTriangle(ox, oy, ix, iy, ix, iy + th, _style.eyeColor);
        _cv->fillTriangle(ox, oy, ix, iy + th, ox, oy + th, _style.eyeColor);
    }
}

void BotUx::_drawMouth() {
    int16_t cx = _m.cx, my = _m.mouthY;
    float er = (float)_m.eyeRadius;
    int16_t mw = (int16_t)(er * 1.6f);
    uint16_t c = _style.mouthColor;

    switch (_effMood) {
        case Mood::Speaking: {
            float a = _talkAmp;
            int16_t rx = (int16_t)(mw * 0.5f * (0.35f + 0.65f * a));
            int16_t ry = (int16_t)(er * (0.25f + 0.85f * a));
            if (ry < 2) ry = 2;
            _cv->fillEllipse(cx, my, rx, ry, c);
            break;
        }
        case Mood::Happy:
            // 0°..180° sweeps right→down→left = bottom arc (a "∪" smile).
            _cv->fillArc(cx, my, (int16_t)(er * 0.7f), (int16_t)(er * 1.6f), 0, 180, c);
            break;
        case Mood::Sad:
            // 180°..360° sweeps left→up→right = top arc (a "∩" frown).
            _cv->fillArc(cx, my, (int16_t)(er * 0.7f), (int16_t)(er * 1.6f), 180, 360, c);
            break;
        case Mood::Surprised: {
            int16_t r = (int16_t)(er * 0.6f);
            _cv->fillCircle(cx, my, r, c);
            _cv->fillCircle(cx, my, (int16_t)(r * 0.45f), _style.bgColor); // "o" ring
            break;
        }
        case Mood::Thinking: {
            int16_t d = (int16_t)(er * 0.42f);
            for (int i = -1; i <= 1; i++)
                _cv->fillCircle(cx + i * (int16_t)(er * 0.5f), my, d, c);
            break;
        }
        case Mood::Listening:
            _cv->fillEllipse(cx, my, (int16_t)(er * 0.45f), (int16_t)(er * 0.30f), c);
            break;
        default: { // Idle, Sleepy — a soft line
            int16_t h = (int16_t)(er * 0.16f);
            if (h < 2) h = 2;
            _cv->fillRoundRect(cx - mw / 2, my - h / 2, mw, h, h / 2, c);
            break;
        }
    }
}

void BotUx::_drawAntenna() {
    int16_t cx = _m.cx, er = _m.eyeRadius;
    int16_t topY;
    if (_style.bodyStyle == BodyStyle::None) {
        topY = _m.eyeY - (int16_t)(er * 2.2f);
    } else {
        topY = _m.cy - _m.bodyR;
    }
    _cv->drawLine(cx, topY, cx, topY - (int16_t)(er * 0.9f), _style.accentColor);
    _cv->fillCircle(cx, topY - (int16_t)(er * 1.0f), (int16_t)(er * 0.32f), _style.accentColor);
}

void BotUx::_drawOverlays() {
    // Signal bars — top-left.
    if (_signal >= 0) {
        for (int i = 0; i < 4; i++) {
            int16_t bh = 3 + i * 2;
            int16_t bx = 4 + i * 5;
            int16_t by = 12 - bh;
            uint16_t c = (i < _signal) ? _style.accentColor : rgb565(0x30, 0x34, 0x3C);
            _cv->fillRect(bx, by, 4, bh, c);
        }
    }

    // Battery — top-right.
    if (_batteryVisible) {
        int16_t bw = (int16_t)(_w * 0.18f);
        if (bw < 18) bw = 18;
        int16_t bh = bw / 2;
        int16_t x0 = _w - bw - 4, y0 = 4;
        _cv->drawRoundRect(x0, y0, bw, bh, 2, _style.accentColor);
        int16_t lvl = (int16_t)((bw - 4) * _battery / 100);
        uint16_t c = (_battery <= 20) ? rgb565(0xFF, 0x4D, 0x4D) : _style.accentColor;
        if (lvl > 0) _cv->fillRect(x0 + 2, y0 + 2, lvl, bh - 4, c);
        _cv->fillRect(x0 + bw, y0 + bh / 4, 2, bh / 2, _style.accentColor); // nub
    }

    // Time + label — bottom centre.
    _cv->setTextDatum(middle_center);
    _cv->setTextColor(_style.accentColor);
    if (_hasTime) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%02u:%02u", _hh, _mm);
        _cv->setTextSize(1.6f);
        _cv->drawString(buf, _w / 2, _h - 18);
    }
    if (_label) {
        _cv->setTextSize(1.0f);
        _cv->setTextColor(_style.eyeColor);
        _cv->drawString(_label, _w / 2, _h - 6);
    }
}

} // namespace botux
