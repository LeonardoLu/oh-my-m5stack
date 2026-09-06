// BotUx — animation implementation. See BotUx.h for the contract.
#include "BotUx.h"

#include <Arduino.h> // millis()
#include <math.h>
#include <stdio.h>

namespace botux {

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr uint32_t BLINK_CLOSE_MS = 90;
constexpr uint32_t BLINK_HOLD_MS  = 50;
constexpr uint32_t BLINK_OPEN_MS  = 100;
constexpr uint32_t SURPRISE_MS    = 240;
constexpr uint32_t HAPPY_MS       = 1180;

uint32_t blinkCloseMs(BotUx::Mood mood) {
    return (mood == BotUx::Mood::Sleepy) ? 180u : BLINK_CLOSE_MS;
}

uint32_t blinkHoldMs(BotUx::Mood mood) {
    return (mood == BotUx::Mood::Sleepy) ? 360u : BLINK_HOLD_MS;
}

uint32_t blinkOpenMs(BotUx::Mood mood) {
    return (mood == BotUx::Mood::Sleepy) ? 220u : BLINK_OPEN_MS;
}

int16_t min16(int16_t a, int16_t b) { return (a < b) ? a : b; }

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float easeAlpha(uint32_t dt, float timeConstantMs) {
    return 1.0f - expf(-(float)dt / timeConstantMs);
}

uint16_t mix565(uint16_t a, uint16_t b, uint8_t amount) {
    uint16_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    uint16_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    uint16_t inv = 255u - amount;
    uint16_t r = (uint16_t)((ar * inv + br * amount) / 255u);
    uint16_t g = (uint16_t)((ag * inv + bg * amount) / 255u);
    uint16_t bl = (uint16_t)((ab * inv + bb * amount) / 255u);
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

BotUx::Metrics computeLayout(int16_t w, int16_t h, const BotUx::Style& s) {
    BotUx::Metrics m;
    int16_t side = min16(w, h);
    int16_t nominalR = (int16_t)(side * 0.39f);
    m.bodyR = (s.bodyStyle == BotUx::BodyStyle::None) ? 0 : nominalR;
    m.eyeRadius = (int16_t)(side * 0.045f * s.eyeSize);
    if (m.eyeRadius < 2) m.eyeRadius = 2;
    m.eyeDX = (int16_t)(nominalR * 0.22f);
    m.cx = w / 2;
    m.cy = (h > (int16_t)(side * 1.30f)) ? (int16_t)(h * 0.39f) : h / 2;
    m.eyeY = m.cy - (int16_t)(nominalR * 0.38f);
    m.mouthY = m.cy + (int16_t)(nominalR * 0.24f);
    return m;
}

uint32_t blinkDelay(const BotUx::Style& s, uint32_t seed) {
    uint32_t lo = s.blinkMinMs;
    uint32_t hi = (s.blinkMaxMs < lo) ? lo : s.blinkMaxMs;
    uint32_t span = hi - lo;
    return lo + (span ? (seed % (span + 1u)) : 0u);
}
} // namespace

void BotUx::begin(M5Canvas* canvas) {
    _cv = canvas;
    if (!_cv) return;
    _w = (int16_t)canvas->width();
    _h = (int16_t)canvas->height();
    _m = computeLayout(_w, _h, _style);
    _animStart = millis();
    _lastNow = 0;
    _nextBlink = _animStart + blinkDelay(_style, _blinkSeed);
    _nextGaze = _animStart + 700u + (_gazeSeed % 800u);
}

void BotUx::setStyle(const Style& s) {
    _style = s;
    _m = computeLayout(_w, _h, _style);
}

void BotUx::setMood(Mood m) { setMood(m, 180); }

void BotUx::setMood(Mood m, uint16_t transitionMs) {
    if (_mood == m) return;
    _mood = m;
    _transitionMs = transitionMs;
    _snapPose = transitionMs == 0;
}

void BotUx::setExpression(Expression expression, uint16_t transitionMs) {
    if (_expression == expression) return;
    _expression = expression;
    _transitionMs = transitionMs;
    _snapPose = transitionMs == 0;
}

void BotUx::setAnimation(Animation animation) { _animation = animation; }

void BotUx::setAnimationSpeed(float speed) {
    _animationSpeed = clampf(speed, 0.25f, 3.0f);
}

void BotUx::setMotionAmount(float amount) {
    _motionAmount = clampf(amount, 0.0f, 2.0f);
}

void BotUx::setReducedMotion(bool reduced) { _reducedMotion = reduced; }

void BotUx::setMotion(float tiltX, float tiltY, float shake) {
    _motionTargetX = clampf(tiltX, -1.0f, 1.0f);
    _motionTargetY = clampf(tiltY, -1.0f, 1.0f);
    _shakeTarget = clampf(shake, 0.0f, 1.0f);
}

void BotUx::seedBlink(uint32_t s) {
    _blinkSeed = s ? s : 0x1234u;
    _gazeSeed = _blinkSeed ^ 0x9E3779B9u;
    uint32_t now = millis();
    _nextBlink = now + blinkDelay(_style, _blinkSeed);
    _nextGaze = now + 700u + (_gazeSeed % 800u);
}

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
    _hh = h % 12;
    if (_hh == 0) _hh = 12;
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
    _updateGaze(_now);
    _updateTalk(_now);
    _resolveMood(_now);
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
    uint32_t closeMs = blinkCloseMs(_effMood);
    uint32_t holdMs = blinkHoldMs(_effMood);
    uint32_t openMs = blinkOpenMs(_effMood);
    if (t < closeMs) _blinkPhase = 0;
    else if (t < closeMs + holdMs) _blinkPhase = 1;
    else if (t < closeMs + holdMs + openMs) _blinkPhase = 2;
    else {
        _blinking = false;
        _blinkSeed = _blinkSeed * 1103515245u + 12345u;
        _nextBlink = now + blinkDelay(_style, _blinkSeed);
    }
}

void BotUx::_updateGaze(uint32_t now) {
    if ((int32_t)(now - _nextGaze) < 0) return;
    _gazeSeed = _gazeSeed * 1664525u + 1013904223u;
    _wanderX = ((float)((_gazeSeed >> 24) & 0xFF) / 127.5f) - 1.0f;
    _gazeSeed = _gazeSeed * 1664525u + 1013904223u;
    _wanderY = ((float)((_gazeSeed >> 24) & 0xFF) / 170.0f) - 0.75f;
    _wanderX = clampf(_wanderX, -0.85f, 0.85f);
    _wanderY = clampf(_wanderY, -0.55f, 0.55f);
    _gazeSeed = _gazeSeed * 1664525u + 1013904223u;
    _nextGaze = now + 2200u + ((_gazeSeed >> 16) % 2600u);
}

void BotUx::_updateTalk(uint32_t now) {
    uint32_t dt = (_lastNow == 0) ? 16u : (now - _lastNow);
    if (dt > 100u) dt = 100u;
    if (!_talking) {
        float a = easeAlpha(dt, 90.0f);
        _talkAmp += (0.0f - _talkAmp) * a;
        if (_talkAmp < 0.01f) _talkAmp = 0.0f;
        return;
    }

    uint32_t t = now - _talkSeed;
    float flap = _pulse(t, 150.0f);
    float syllable = 0.35f + 0.65f * _pulse(t, 390.0f, 1.2f);
    _talkAmp = 0.18f + 0.82f * flap * syllable;
}

void BotUx::_resolveMood(uint32_t now) {
    if (_pokeUntil && (int32_t)(_pokeUntil - now) > 0) _effMood = Mood::Surprised;
    else if (_reactionUntil && (int32_t)(_reactionUntil - now) > 0) _effMood = Mood::Happy;
    else {
        _effMood = _mood;
        _pokeUntil = 0;
        _reactionUntil = 0;
    }

    if (_effMood == Mood::Surprised) _effExpression = Expression::Alarmed;
    else if (_effMood == Mood::Happy) _effExpression = Expression::Joy;
    else if (_expression != Expression::Auto) _effExpression = _expression;
    else {
        switch (_effMood) {
            case Mood::Listening: _effExpression = Expression::Curious; break;
            case Mood::Working:   _effExpression = Expression::Focused; break;
            case Mood::Done:      _effExpression = Expression::Bashful; break;
            default:              _effExpression = Expression::Neutral; break;
        }
    }

    float targetOpen = 1.0f, targetAsym = 0.04f;
    float targetPairX = 0.26f, targetPairY = -0.38f, targetAngle = 0.34f;
    float targetLean = 0.0f, targetTwist = 0.0f;
    float gazeX = _wanderX, gazeY = _wanderY;
    float lift = 0.0f, stretch = 0.0f;

    switch (_effMood) {
        case Mood::Listening:
            targetOpen = 1.12f; targetAsym = 0.0f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.24f;
            gazeX = 0.0f; gazeY = 0.0f; lift = -0.02f;
            break;
        case Mood::Thinking:
            targetOpen = 0.84f; targetAsym = 0.25f;
            gazeX = -0.65f; gazeY = -0.45f; targetLean = -0.12f;
            break;
        case Mood::Speaking:
            targetOpen = 0.94f; targetAsym = 0.06f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.18f;
            gazeX = 0.0f; gazeY = 0.0f;
            break;
        case Mood::Happy:
            targetOpen = 0.66f; targetAsym = 0.0f;
            targetPairX = 0.18f; targetPairY = -0.25f;
            gazeX = 0.0f; gazeY = 0.0f; lift = -0.05f; stretch = -0.025f;
            break;
        case Mood::Sad:
            targetOpen = 0.48f; targetAsym = 0.12f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.25f;
            gazeX = -0.10f; gazeY = 0.48f; lift = 0.045f; stretch = -0.035f;
            break;
        case Mood::Sleepy:
            targetOpen = 0.11f; targetAsym = 0.10f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.28f;
            gazeX = -0.18f; gazeY = 0.30f; lift = 0.055f; stretch = -0.045f;
            break;
        case Mood::Waiting:
            targetOpen = 0.16f; targetAsym = 0.14f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.30f;
            gazeX = 0.0f; gazeY = 0.0f; lift = 0.015f; stretch = -0.015f;
            break;
        case Mood::Surprised:
            targetOpen = 1.16f; targetAsym = 0.0f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.05f;
            gazeX = 0.0f; gazeY = 0.0f; lift = -0.055f; stretch = 0.08f;
            break;
        case Mood::Working:
            targetOpen = 0.96f; targetAsym = 0.03f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.10f;
            gazeX = 0.12f; gazeY = 0.0f;
            break;
        case Mood::Blocked:
            targetOpen = 1.0f; targetAsym = 0.0f;
            gazeX = 0.0f; gazeY = 0.0f; stretch = 0.04f;
            break;
        case Mood::Done:
            targetOpen = 0.88f; targetAsym = -0.06f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.28f;
            gazeX = -0.22f; gazeY = 0.15f; lift = -0.025f;
            break;
        default:
            break;
    }

    // Explicit expressions replace face geometry while retaining the mood's
    // body rhythm. Keep the same eye spacing and upper-right face placement across poses.
    switch (_effExpression) {
        case Expression::Neutral:
            if (_expression == Expression::Neutral) {
                targetOpen = 1.0f; targetAsym = 0.04f;
                targetPairX = 0.26f; targetPairY = -0.38f; targetAngle = 0.34f;
                targetLean = 0.0f; targetTwist = 0.0f;
                gazeX = _wanderX; gazeY = _wanderY;
            }
            break;
        case Expression::Curious:
            targetOpen = 1.06f; targetAsym = 0.08f;
            targetPairX = 0.18f; targetPairY = -0.27f; targetAngle = 0.22f;
            targetLean = -0.12f; gazeX = 0.48f; gazeY = -0.22f;
            break;
        case Expression::Focused:
            targetOpen = 0.70f; targetAsym = 0.0f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.22f;
            gazeX = 0.0f; gazeY = 0.08f;
            break;
        case Expression::Joy:
            targetOpen = 0.66f; targetAsym = 0.0f;
            targetPairX = 0.18f; targetPairY = -0.25f;
            gazeX = 0.0f; gazeY = 0.0f;
            break;
        case Expression::Skeptical:
            targetOpen = 0.86f; targetAsym = -0.14f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.28f;
            targetLean = 0.16f; gazeX = -0.54f; gazeY = -0.10f;
            break;
        case Expression::Bashful:
            targetOpen = 0.86f; targetAsym = 0.12f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.34f;
            gazeX = -0.34f; gazeY = 0.46f; targetTwist = -0.08f;
            break;
        case Expression::Wink:
            targetOpen = 0.80f; targetAsym = 0.0f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.18f;
            targetLean = -0.15f; gazeX = 0.24f; gazeY = 0.0f;
            break;
        case Expression::Dizzy:
            targetOpen = 0.92f; targetAsym = 0.0f;
            targetPairX = 0.18f; targetPairY = -0.25f;
            targetTwist = sinf(2.0f * kPi * (now - _animStart) / 3600.0f) * 0.08f;
            gazeX = 0.0f; gazeY = 0.0f;
            break;
        case Expression::Alarmed:
            targetOpen = 1.16f; targetAsym = 0.0f;
            targetPairX = 0.18f; targetPairY = -0.25f; targetAngle = 0.05f;
            gazeX = 0.0f; gazeY = 0.0f;
            break;
        default:
            break;
    }

    if (_battery <= 10 && targetOpen > 0.24f) {
        targetOpen = 0.24f;
        lift += 0.03f;
    }

    uint32_t dt = (_lastNow == 0) ? 16u : (now - _lastNow);
    if (dt > 1000u) dt = 1000u;
    float timeConstant = (_transitionMs == 0) ? 1.0f : clampf(_transitionMs * 0.42f, 28.0f, 600.0f);
    float a = _snapPose ? 1.0f : easeAlpha(dt, timeConstant);
    _openBase += (targetOpen - _openBase) * a;
    _eyeAsym += (targetAsym - _eyeAsym) * a;
    _eyeSmile += ((_effExpression == Expression::Joy ? 1.0f : 0.0f) - _eyeSmile) * a;
    _eyeWink += ((_effExpression == Expression::Wink ? 1.0f : 0.0f) - _eyeWink) * a;
    _eyeRound += ((_effExpression == Expression::Alarmed ? 0.50f : 0.0f) - _eyeRound) * a;
    _eyePairX += (targetPairX - _eyePairX) * a;
    _eyePairY += (targetPairY - _eyePairY) * a;
    _eyeAngle += (targetAngle - _eyeAngle) * a;
    _bodyLean += (targetLean - _bodyLean) * a;
    _eyeTwist += (targetTwist - _eyeTwist) * a;
    _pupilDX += (gazeX - _pupilDX) * a;
    _pupilDY += (gazeY - _pupilDY) * a;
    _snapPose = false;

    float motionA = easeAlpha(dt, _reducedMotion ? 380.0f : 240.0f);
    _motionX += (_motionTargetX - _motionX) * motionA;
    _motionY += (_motionTargetY - _motionY) * motionA;
    _shake += (_shakeTarget - _shake) * motionA;

    float open = _openBase;
    if (_blinking && _effMood != Mood::Thinking && _effMood != Mood::Blocked) {
        uint32_t t = now - _blinkStart;
        uint32_t closeMs = blinkCloseMs(_effMood);
        uint32_t holdMs = blinkHoldMs(_effMood);
        uint32_t openMs = blinkOpenMs(_effMood);
        float p;
        if (_blinkPhase == 0) p = (float)t / closeMs;
        else if (_blinkPhase == 1) p = 1.0f;
        else p = 1.0f - (float)(t - closeMs - holdMs) / openMs;
        open *= 1.0f - _smooth(clampf(p, 0.0f, 1.0f));
    }
    _open = clampf(open, 0.0f, 1.35f);

    uint32_t elapsed = now - _animStart;
    float animElapsed = elapsed * _animationSpeed;
    float period = (_effMood == Mood::Sleepy) ? 7200.0f
                 : (_effMood == Mood::Waiting) ? 4600.0f : 3800.0f;
    _breath = sinf(2.0f * kPi * animElapsed / period);
    int16_t r = _m.bodyR ? _m.bodyR : (int16_t)(min16(_w, _h) * 0.39f);
    float dx = 0.0f;
    float dy = lift * r + _breath * r * 0.018f;
    float sx = 1.0f + _breath * 0.018f;
    float sy = 1.0f - _breath * 0.012f + stretch;
    switch (_effMood) {
        case Mood::Thinking:
            dx += sinf(2.0f * kPi * animElapsed / 2300.0f) * r * 0.055f;
            break;
        case Mood::Speaking:
            dy -= _talkAmp * r * 0.045f;
            sx += _talkAmp * 0.035f;
            sy += _talkAmp * 0.055f;
            break;
        case Mood::Working:
            dx += sinf(2.0f * kPi * animElapsed / 900.0f) * r * 0.025f;
            sy += _pulse((uint32_t)animElapsed, 520.0f) * 0.035f;
            break;
        case Mood::Happy:
        case Mood::Done:
            dy -= _pulse((uint32_t)animElapsed, 780.0f) * r * 0.025f;
            break;
        case Mood::Surprised:
            sy += _pulse((uint32_t)animElapsed, 260.0f) * 0.025f;
            sx -= 0.025f;
            break;
        default:
            break;
    }

    Animation active = _animation;
    if (active == Animation::Auto) {
        switch (_effMood) {
            case Mood::Listening: active = Animation::Curious; break;
            case Mood::Thinking:
            case Mood::Working:   active = Animation::Orbit; break;
            case Mood::Speaking:
            case Mood::Surprised: active = Animation::Bounce; break;
            case Mood::Happy:
            case Mood::Done:      active = Animation::Sparkle; break;
            case Mood::Waiting:   active = Animation::Wave; break;
            case Mood::Blocked:   active = Animation::Glitch; break;
            default:              active = Animation::Calm; break;
        }
    }
    _activeAnimation = active;

    float amount = (_reducedMotion ? 0.10f : 0.45f) * _motionAmount;
    float phase = 2.0f * kPi * animElapsed;
    float animEyeTwist = 0.0f;
    switch (active) {
        case Animation::Curious:
            dx += sinf(phase / 1900.0f) * r * 0.050f * amount;
            dy += cosf(phase / 2500.0f) * r * 0.025f * amount;
            animEyeTwist = sinf(phase / 2100.0f) * 0.12f * amount;
            break;
        case Animation::Orbit:
            dx += sinf(phase / 1300.0f) * r * 0.065f * amount;
            dy += cosf(phase / 1300.0f) * r * 0.045f * amount;
            animEyeTwist = sinf(phase / 1300.0f) * 0.18f * amount;
            break;
        case Animation::Bounce: {
            float hop = _pulse((uint32_t)animElapsed, 760.0f);
            dy -= hop * hop * r * 0.10f * amount;
            sx += (0.5f - hop) * 0.065f * amount;
            sy -= (0.5f - hop) * 0.085f * amount;
            break;
        }
        case Animation::Glitch: {
            int step = (int)(animElapsed / 95.0f);
            int jitter = ((step * 37 + 11) % 7) - 3;
            dx += jitter * r * 0.018f * amount;
            animEyeTwist = (((step * 13) % 5) - 2) * 0.055f * amount;
            break;
        }
        case Animation::Wave:
            dx += sinf(phase / 1500.0f) * r * 0.075f * amount;
            animEyeTwist = cosf(phase / 1500.0f) * 0.24f * amount;
            break;
        case Animation::Sparkle: {
            float glow = _pulse((uint32_t)animElapsed, 900.0f, 0.8f);
            dy -= _pulse((uint32_t)animElapsed, 1100.0f) * r * 0.035f * amount;
            sx += glow * 0.025f * amount;
            sy += glow * 0.025f * amount;
            break;
        }
        default:
            break;
    }
    _animEyeTwist = animEyeTwist;

    // Tilt moves the whole character while the eyes counter-shift slightly,
    // creating depth. Shake produces a brief deterministic squash/jitter.
    float sensorAmount = _reducedMotion ? _motionAmount * 0.32f : _motionAmount;
    dx += _motionX * r * 0.055f * sensorAmount;
    dy += _motionY * r * 0.042f * sensorAmount;
    if (!_reducedMotion && _shake > 0.01f) {
        float shakeWave = sinf(phase / 72.0f);
        dx += shakeWave * _shake * r * 0.025f * _motionAmount;
        sx += _shake * 0.055f * _motionAmount;
        sy -= _shake * 0.060f * _motionAmount;
    }
    _bodySX += (clampf(sx, 0.88f, 1.12f) - _bodySX) * a;
    _bodySY += (clampf(sy, 0.86f, 1.14f) - _bodySY) * a;
    // Preserve the silhouette at motion extremes. This matters most for a
    // large watch-face sprite where only a narrow transparent-looking margin
    // surrounds the orb (the sprite itself is opaque RGB565).
    float rx = r * _bodySX;
    float ry = r * _bodySY;
    dx = clampf(dx, rx - _m.cx + 2.0f, _w - _m.cx - rx - 2.0f);
    dy = clampf(dy, ry - _m.cy + 2.0f, _h - _m.cy - ry - 2.0f);
    _bodyDX += (dx - _bodyDX) * a;
    _bodyDY += (dy - _bodyDY) * a;
}

void BotUx::draw() {
    if (!_cv) return;
    _cv->fillSprite(_style.bgColor);
    _drawBody();
    _drawAnimationFx();
    _drawEyes();
    _drawOverlays();
}

void BotUx::_drawBody() {
    int16_t r = _m.bodyR ? _m.bodyR : (int16_t)(min16(_w, _h) * 0.39f);
    int16_t cx = _m.cx + (int16_t)_bodyDX;
    int16_t cy = _m.cy + (int16_t)_bodyDY;
    int16_t rx = (int16_t)(r * _bodySX);
    int16_t ry = (int16_t)(r * _bodySY);
    if (rx < 2) rx = 2;
    if (ry < 2) ry = 2;

    // Thinking and Blocked replace the avatar silhouette in the official motion language.
    if (_effMood == Mood::Thinking) {
        int16_t dotR = (int16_t)(r * 0.19f);
        if (dotR < 2) dotR = 2;
        for (int i = -1; i <= 1; ++i) {
            float phase = i * 1.35f;
            float elapsed = (_now - _animStart) * _animationSpeed;
            int16_t y = cy + (int16_t)(sinf(2.0f * kPi * elapsed / 920.0f + phase) * r * 0.13f);
            uint8_t mix = (uint8_t)(35 + (i + 1) * 25);
            _cv->fillCircle(cx + i * (int16_t)(r * 0.48f), y, dotR,
                            mix565(_style.bodyColor, _style.bgColor, mix));
        }
        return;
    }
    if (_effMood == Mood::Blocked) {
        int16_t stemW = (int16_t)(r * 0.28f);
        int16_t stemH = (int16_t)(r * 0.98f);
        if (stemW < 3) stemW = 3;
        _cv->fillRoundRect(cx - stemW / 2, cy - (int16_t)(r * 0.63f), stemW, stemH,
                           stemW / 2, _style.bodyColor);
        _cv->fillCircle(cx, cy + (int16_t)(r * 0.58f), stemW / 2, _style.bodyColor);
        return;
    }
    if (_style.bodyStyle == BodyStyle::None) return;

    switch (_style.bodyStyle) {
        case BodyStyle::Round:
            _fillEllipseAA(_m.cx + _bodyDX, _m.cy + _bodyDY,
                           r * _bodySX, r * _bodySY, _style.bodyColor);
            break;
        case BodyStyle::RoundedSquare: {
            int16_t corner = (int16_t)(r * 0.48f);
            _cv->fillRoundRect(cx - rx, cy - ry, rx * 2, ry * 2, corner, _style.bodyColor);
            break;
        }
        case BodyStyle::Hexagon: {
            int16_t px[6], py[6];
            for (int i = 0; i < 6; ++i) {
                float angle = kPi / 6.0f + i * kPi / 3.0f;
                px[i] = cx + (int16_t)(rx * cosf(angle));
                py[i] = cy + (int16_t)(ry * sinf(angle));
            }
            for (int i = 0; i < 6; ++i)
                _cv->fillTriangle(cx, cy, px[i], py[i], px[(i + 1) % 6], py[(i + 1) % 6], _style.bodyColor);
            break;
        }
        default:
            break;
    }
}

void BotUx::_drawAnimationFx() {
    if (_activeAnimation != Animation::Sparkle || _reducedMotion ||
        _m.bodyR == 0 || _motionAmount <= 0.0f) return;
    int16_t r = _m.bodyR;
    int16_t cx = _m.cx + (int16_t)_bodyDX;
    int16_t cy = _m.cy + (int16_t)_bodyDY;
    float elapsed = (_now - _animStart) * _animationSpeed;
    for (int i = 0; i < 3; ++i) {
        float angle = 2.0f * kPi * elapsed / 2400.0f + i * 2.094f;
        float pulse = 0.65f + 0.35f * sinf(2.0f * kPi * elapsed / 780.0f + i);
        int16_t sr = (int16_t)(r * 0.035f * pulse * _motionAmount);
        if (sr < 1) sr = 1;
        int16_t x = cx + (int16_t)(cosf(angle) * r * 1.12f);
        int16_t y = cy + (int16_t)(sinf(angle) * r * 0.88f);
        if (x < sr) x = sr;
        if (x > _w - sr - 1) x = _w - sr - 1;
        if (y < sr) y = sr;
        if (y > _h - sr - 1) y = _h - sr - 1;
        _cv->fillCircle(x, y, sr, _style.accentColor);
    }
}

// Scanline ellipse: solid spans plus one-pixel coverage at the perimeter.
// The extra work scales with circumference, with no supersampled framebuffer.
void BotUx::_fillEllipseAA(float cx, float cy, float rx, float ry, uint16_t color) {
    for (int y = (int)floorf(cy - ry - 0.5f); y <= (int)ceilf(cy + ry + 0.5f); ++y) {
        float extent[4], widest = 0.0f, narrowest = rx;
        for (int sample = 0; sample < 4; ++sample) {
            float v = (y - cy + (sample - 1.5f) * 0.25f) / ry;
            extent[sample] = fabsf(v) >= 1.0f ? 0.0f : rx * sqrtf(1.0f - v * v);
            widest = fmaxf(widest, extent[sample]);
            narrowest = fminf(narrowest, extent[sample]);
        }
        if (widest == 0.0f) continue;
        int left = (int)ceilf(cx - narrowest + 0.5f);
        int right = (int)floorf(cx + narrowest - 0.5f);
        if (right >= left) _cv->fillRect(left, y, right - left + 1, 1, color);
        for (int side = -1; side <= 1; side += 2) {
            int begin = side < 0 ? (int)floorf(cx - widest - 0.5f) : right + 1;
            int end = side < 0 ? left - 1 : (int)ceilf(cx + widest + 0.5f);
            for (int x = begin; x <= end; ++x) {
                float coverage = 0.0f;
                for (int sample = 0; sample < 4; ++sample)
                    coverage += fmaxf(0.0f, fminf(x + 0.5f, cx + extent[sample]) -
                                               fmaxf(x - 0.5f, cx - extent[sample]));
                if (coverage > 0.0f)
                    _cv->drawPixel(x, y, mix565(_style.bgColor, color, (uint8_t)(coverage * 63.75f)));
            }
        }
    }
}

void BotUx::_fillCapsule(float cx, float cy, float halfDx, float halfDy,
                         float radius, uint16_t color) {
    radius = fmaxf(radius, 0.8f);
    float ax = cx - halfDx, ay = cy - halfDy;
    float vx = halfDx * 2.0f, vy = halfDy * 2.0f;
    float invLen = 1.0f / fmaxf(vx * vx + vy * vy, 0.0001f);
    int minX = (int)floorf(cx - fabsf(halfDx) - radius - 0.5f);
    int maxX = (int)ceilf(cx + fabsf(halfDx) + radius + 0.5f);
    int minY = (int)floorf(cy - fabsf(halfDy) - radius - 0.5f);
    int maxY = (int)ceilf(cy + fabsf(halfDy) + radius + 0.5f);
    float inner2 = (radius - 0.5f) * (radius - 0.5f);
    float outer2 = (radius + 0.5f) * (radius + 0.5f);
    for (int y = minY; y <= maxY; ++y) {
        int run = -1;
        for (int x = minX; x <= maxX; ++x) {
            float t = clampf(((x - ax) * vx + (y - ay) * vy) * invLen, 0.0f, 1.0f);
            float dx = x - ax - t * vx, dy = y - ay - t * vy;
            float d2 = dx * dx + dy * dy;
            if (d2 <= inner2) {
                if (run < 0) run = x;
                continue;
            }
            if (run >= 0) { _cv->fillRect(run, y, x - run, 1, color); run = -1; }
            if (d2 >= outer2) continue;
            uint8_t coverage = (uint8_t)(clampf(radius + 0.5f - sqrtf(d2), 0.0f, 1.0f) * 255.0f);
            uint16_t under = _cv->readPixel(x, y);
            _cv->drawPixel(x, y, mix565(under, color, coverage));
        }
        if (run >= 0) _cv->fillRect(run, y, maxX - run + 1, 1, color);
    }
}

void BotUx::_drawEyes() {
    if (_effMood == Mood::Thinking || _effMood == Mood::Blocked) return;
    int16_t side = min16(_w, _h);
    int16_t r = _m.bodyR ? _m.bodyR : (int16_t)(side * 0.39f);
    float bodyCx = _m.cx + _bodyDX;
    float bodyCy = _m.cy + _bodyDY;
    float sensorAmount = _reducedMotion ? _motionAmount * 0.32f : _motionAmount;
    float gazeX = clampf(_pupilDX - _motionX * 0.18f * sensorAmount, -1.0f, 1.0f);
    float gazeY = clampf(_pupilDY - _motionY * 0.14f * sensorAmount, -1.0f, 1.0f);
    float pairCx = bodyCx + _eyePairX * r + gazeX * r * 0.075f;
    float pairCy = bodyCy + _eyePairY * r + gazeY * r * 0.075f;
    int16_t dx = _m.eyeDX;
    float er = (float)_m.eyeRadius;

    float twist = _eyeTwist + _animEyeTwist;
    float twistCos = cosf(twist), twistSin = sinf(twist);
    for (int s = -1; s <= 1; s += 2) {
        float asym = 1.0f + s * _eyeAsym;
        float eyeOpen = clampf(_open * asym, 0.05f, 1.35f);
        float pairX = s * dx;
        float pairY = s * _bodyLean * er;
        float ex = pairCx + pairX * twistCos - pairY * twistSin;
        float ey = pairCy + pairX * twistSin + pairY * twistCos;

        if (_expression == Expression::Auto && _effMood == Mood::Waiting && side <= 96) {
            int16_t halfW = (side <= 48) ? 1 : 2;
            _fillCapsule(ex, ey, halfW, 0, 1, _style.eyeColor);
            continue;
        }

        float lengthScale = 1.0f, radiusScale = 0.52f;
        switch (_style.eyeStyle) {
            case EyeStyle::Round:  lengthScale = 0.88f; radiusScale = 0.62f; break;
            case EyeStyle::Oval:   lengthScale = 1.25f; radiusScale = 0.48f; break;
            case EyeStyle::Square: lengthScale = 0.86f; radiusScale = 0.72f; break;
            case EyeStyle::Googly: lengthScale = 0.12f; radiusScale = 1.02f; break;
        }

        // Morph the neutral pill into a short, rounded arch. A blink closes
        // each eye continuously; a wink closes only the left eye.
        float closure = clampf(eyeOpen / 0.45f, 0.0f, 1.0f);
        if (s < 0) closure *= 1.0f - _eyeWink;
        float smile = _eyeSmile * closure;
        float major = er * lengthScale * (0.62f + 0.38f * eyeOpen) *
                      (1.0f - _eyeRound) * closure * (1.0f - smile);
        float baseDy = major / sqrtf(1.0f + _eyeAngle * _eyeAngle);
        float baseDx = baseDy * _eyeAngle;
        float halfDx = baseDx * twistCos - baseDy * twistSin;
        float halfDy = baseDx * twistSin + baseDy * twistCos;
        float radius = fmaxf(0.9f, er * radiusScale * eyeOpen * closure);
        radius = radius * (1.0f - smile) + er * 0.27f * smile;
        float closedWidth = er * 0.60f * (1.0f - closure);
        halfDx += closedWidth;
        if (smile > 0.01f) {
            // Three joined capsules form a shallow smile, with rounded ends.
            const float width = er * 0.90f * smile;
            const float rise = er * 0.46f * smile;
            _fillCapsule(ex - width * 0.65f, ey - rise * 0.5f,
                         width * 0.35f + halfDx, -rise * 0.5f + halfDy, radius, _style.eyeColor);
            _fillCapsule(ex, ey - rise, width * 0.30f + halfDx,
                         halfDy, radius, _style.eyeColor);
            _fillCapsule(ex + width * 0.65f, ey - rise * 0.5f,
                         width * 0.35f + halfDx, rise * 0.5f + halfDy, radius, _style.eyeColor);
        } else {
            _fillCapsule(ex, ey, halfDx, halfDy, radius, _style.eyeColor);
        }

        if (_style.eyeStyle == EyeStyle::Googly && eyeOpen > 0.34f) {
            int16_t pr = (int16_t)(er * 0.34f);
            if (pr < 1) pr = 1;
            _cv->fillCircle(ex + (int16_t)(_pupilDX * er * 0.32f),
                            ey + (int16_t)(_pupilDY * er * 0.32f), pr, _style.pupilColor);
        }
    }
}

void BotUx::_drawOverlays() {
    if (_signal >= 0) {
        for (int i = 0; i < 4; ++i) {
            int16_t bh = 3 + i * 2;
            int16_t bx = 4 + i * 5;
            int16_t by = 12 - bh;
            uint16_t c = (i < _signal) ? _style.accentColor : mix565(_style.bgColor, _style.accentColor, 44);
            _cv->fillRect(bx, by, 4, bh, c);
        }
    }

    if (_batteryVisible) {
        int16_t bw = (int16_t)(_w * 0.18f);
        if (bw < 18) bw = 18;
        int16_t bh = bw / 2;
        int16_t x0 = _w - bw - 4, y0 = 4;
        _cv->drawRoundRect(x0, y0, bw, bh, 2, _style.accentColor);
        int16_t lvl = (int16_t)((bw - 4) * _battery / 100);
        uint16_t c = (_battery <= 20) ? rgb565(0xFF, 0x4D, 0x4D) : _style.accentColor;
        if (lvl > 0) _cv->fillRect(x0 + 2, y0 + 2, lvl, bh - 4, c);
        _cv->fillRect(x0 + bw, y0 + bh / 4, 2, bh / 2, _style.accentColor);
    }

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
