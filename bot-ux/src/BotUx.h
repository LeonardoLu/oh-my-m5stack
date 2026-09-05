// BotUx — a small, controllable "grok bot" animation component.
//
// The component renders a character (face + body + optional status ring) into a
// caller-provided M5Canvas sprite. It never owns the display: the host creates
// the sprite, calls begin(), drives update()/draw() each frame, and pushes the
// sprite to its display when it pleases. This keeps the component embeddable
// into both a watch (StickC Plus2) and a touch tablet (Core2) with zero display
// coupling.
//
// Animation is time-based (millis), framerate-agnostic, and driven by a small
// state machine per feature (blink, pupil drift, mouth, breathing, reactions).
#pragma once

#include <M5GFX.h> // M5Canvas (== LGFX_Sprite); lighter than pulling all of M5Unified
#include <stdint.h>

namespace botux {

// ---- color helpers -------------------------------------------------------
// M5GFX stores pixels as RGB565. This keeps defaults self-contained and avoids
// depending on the TFT_* macro set.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

class BotUx {
public:
    enum class Mood : uint8_t {
        Idle = 0,   // neutral, breathing, periodic blinks
        Listening,  // engaged, eyes widen, subtle nod
        Thinking,   // eyes up, brows knit, "..." indicator
        Speaking,   // mouth animates (driven by setTalking)
        Happy,      // ^ ^ eyes + big smile
        Sad,        // drooped lids + frown
        Sleepy,     // half/closed eyes, slow breath
        Surprised,  // wide eyes + O mouth (transient)
    };

    enum class EyeStyle : uint8_t {
        Round = 0, // classic circle eye
        Oval,      // taller ellipse
        Square,    // rounded square
        Googly,    // white ball + free-floating pupil
    };

    enum class BodyStyle : uint8_t {
        None = 0,        // face elements only (no body silhouette)
        Round,           // circle body behind the face
        RoundedSquare,   // app-like tile body
        Hexagon,         // six-sided body
    };

    // Everything that makes one bot "this bot".
    struct Style {
        uint16_t bgColor      = rgb565(0x0A, 0x0E, 0x14); // deep space blue
        uint16_t bodyColor    = rgb565(0x2A, 0x2E, 0x38); // slate
        uint16_t accentColor  = rgb565(0x2E, 0xD9, 0xC8); // teal glow
        uint16_t eyeColor     = rgb565(0xFF, 0xFF, 0xFF);
        uint16_t pupilColor   = rgb565(0x0A, 0x12, 0x22);
        uint16_t mouthColor   = rgb565(0xFF, 0xFF, 0xFF);
        uint16_t blushColor   = rgb565(0xFF, 0x8A, 0xA0);

        EyeStyle eyeStyle   = EyeStyle::Round;
        BodyStyle bodyStyle = BodyStyle::Round;

        float    eyeSize    = 1.0f;   // 0.5..2.0, scales eye radius
        uint32_t blinkMinMs = 2200;   // min time between blinks
        uint32_t blinkMaxMs = 5200;   // max time between blinks
    };

    // Computed layout, exposed so hosts can position overlays (e.g. a clock
    // label, battery bar) relative to the face.
    struct Metrics {
        int16_t cx = 0, cy = 0;   // face centre
        int16_t eyeY = 0;         // eye baseline
        int16_t eyeDX = 0;        // horizontal half-distance between eyes
        int16_t eyeRadius = 0;    // base eye radius (px)
        int16_t mouthY = 0;       // mouth baseline
        int16_t bodyR = 0;        // body radius (0 if BodyStyle::None)
    };

    BotUx() = default;

    // Bind a render target. The sprite must already be created (createSprite)
    // at 16-bit RGB565; the component reads its size from the canvas. The canvas
    // must outlive the BotUx.
    void begin(M5Canvas* canvas);

    // ---- configuration ----------------------------------------------------
    void setStyle(const Style& s);
    const Style& style() const { return _style; }

    // Persistent mood. Transient reactions (poke) override for their duration
    // then return to this mood.
    void setMood(Mood m);

    // Drive the mouth when Speaking, or add a talk overlay otherwise.
    void setTalking(bool on);

    // 0..100 battery. <=10% forces a sleepy droop regardless of mood.
    // Setting a value also makes the battery icon visible.
    void setBattery(uint8_t pct);
    void setBatteryVisible(bool vis); // show/hide the icon independently

    // Signal strength: -1 hides, 0..4 shows bars near the antenna.
    void setSignal(int8_t bars);

    // Optional clock text drawn under the face (e.g. "09:41"). h is 1..12; pm
    // selects AM/PM formatting (only used to echo an "AM/PM" suffix by the host).
    void setTime(uint8_t h, uint8_t m, uint8_t s, bool pm);
    // Single-line caption, nullptr hides. The string must stay valid until the
    // next setLabel() or BotUx destruction — pass a static/persistent buffer,
    // never a String().c_str() or stack temporary.
    void setLabel(const char* text);

    // ---- interaction ------------------------------------------------------
    // Surprise for ~600ms then settle into Happy for ~1200ms, then prior mood.
    void poke();

    // ---- frame loop -------------------------------------------------------
    // Advance animation. Pass 0 (or nothing) to use millis() internally.
    void update(uint32_t nowMs = 0);
    void draw(); // render the current frame into the bound canvas

    const Metrics& metrics() const { return _m; }

    // Nudge RNG state (e.g. from ADC) so two bots don't blink in lockstep.
    void seedBlink(uint32_t s) { _blinkSeed = s; }

private:
    // animation state
    void _updateBlink(uint32_t now);
    void _updatePupil(uint32_t now);
    void _updateTalk(uint32_t now);
    void _resolveMood(uint32_t now); // set _effMood + _open from mood/reaction/battery

    float _smooth(float t) const { return t * t * (3.0f - 2.0f * t); } // smoothstep
    float _pulse(uint32_t ms, float period, float phase = 0.0f) const; // 0..1

    void _drawBody();
    void _drawEyes();
    void _drawMouth();
    void _drawBrows();
    void _drawAntenna();
    void _drawOverlays(); // battery/signal/label/time

    M5Canvas* _cv = nullptr;
    int16_t _w = 0, _h = 0;
    Style _style;
    Metrics _m;

    Mood _mood = Mood::Idle;

    bool   _talking = false;
    uint8_t _battery = 100;  // 0..100
    bool   _batteryVisible = false;
    int8_t  _signal = -1;    // -1 hidden, 0..4
    bool   _hasTime = false;
    uint8_t _hh = 0, _mm = 0, _ss = 0;
    bool   _pm = false;
    const char* _label = nullptr;

    // per-frame derived state (computed in update(), consumed by draw())
    uint32_t _now = 0;
    uint32_t _lastNow = 0;   // previous update() timestamp, for framerate-independent easing
    Mood   _effMood = Mood::Idle;
    float  _openBase = 1.0f; // eased base openness (mood/sleep), before blink
    float  _open = 1.0f;     // eye openness 0..1 (blink + sleep + mood)
    float  _pupilDX = 0.0f;  // -1..1, fraction of eyeRadius
    float  _pupilDY = 0.0f;
    float  _talkAmp = 0.0f;  // 0..1 mouth amplitude
    float  _breath = 0.0f;   // -1..1 body breathing

    // blink
    uint32_t _blinkSeed = 0x1234;
    uint32_t _nextBlink = 0;
    uint32_t _blinkStart = 0;
    bool     _blinking = false;
    uint8_t  _blinkPhase = 0; // 0 closing, 1 closed, 2 opening

    // reactions
    uint32_t _reactionUntil = 0; // happy phase end (0 = none)
    uint32_t _pokeUntil = 0;     // surprise phase end

    uint32_t _animStart = 0;     // fixed origin for deterministic drift
    uint32_t _talkSeed = 0;
};

} // namespace botux
