# bot-ux

A small, controllable Grok-inspired bot animation component for M5Stack devices.

It draws a minimal orb and two expressive eye marks into a
caller-provided [`M5Canvas`](https://docs.m5stack.com/en/arduino/m5stack/m5gfx)
sprite. The component never touches the display — the host creates the sprite,
drives `update()`/`draw()` each frame, and pushes the sprite when convenient.
That keeps it embeddable into both a watch (M5Stack StopWatch) and a touch tablet
(Core2) with no display coupling.

## Quick start

```cpp
#include <M5Unified.h>
#include <BotUx.h>

M5Canvas canvas(&M5.Display);
botux::BotUx bot;

void setup() {
  M5.begin();
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  bot.begin(&canvas);
  bot.setBattery(87);
  bot.setBatteryVisible(false); // let the host own status UI
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) bot.poke();   // surprise -> happy reaction
  bot.update(millis());
  bot.draw();
  canvas.pushSprite(0, 0);
}
```

## API (see `src/BotUx.h`)

- **Moods** — the original `Idle`, `Listening`, `Thinking`, `Speaking`, `Happy`,
  `Sad`, `Sleepy`, and `Surprised`, plus additive task states `Working`, `Waiting`,
  `Blocked`, and `Done` via `setMood()`.
- **Personalization** — `Style` holds the background, orb, eye, and status colors;
  eye mark style; silhouette style; eye scale; and blink rate. `Round` + `Oval`
  is the closest match to the default Grok visual language.
- **Expressions** — `setExpression()` selects `Neutral`, `Curious`, `Focused`,
  `Joy`, `Skeptical`, `Bashful`, `Wink`, `Dizzy`, or `Alarmed`, independently
  of lifecycle mood. `Auto` follows the mood. Pass a transition duration, or
  zero to switch directly on the next update.
- **Choreography** — `setAnimation()` selects `Calm`, `Curious`, `Orbit`,
  `Bounce`, `Glitch`, `Wave`, or `Sparkle`. `Auto` assigns distinct motion to
  each lifecycle state. `setAnimationSpeed()`, `setMotionAmount()`, and
  `setReducedMotion()` are suitable for device settings.
- **Status** — `setBattery`, `setSignal`, `setTime`, `setLabel`.
- **Interaction** — `poke()` (quick surprise→happy), `setTalking(bool)` to
  drive the speaking pulse, and `setMotion(tiltX, tiltY, shake)` for normalized
  IMU input.
- **Frame loop** — `update(nowMs)` then `draw()`.

The component renders into whatever canvas you give it and scales itself to
that canvas, so a bot can be a watch-face hero or a small command-deck tile.
Its draw path uses fixed stack locals and M5GFX primitives, with no per-frame
heap allocation.

The visual system follows xAI's published [Grok Bot design notes](https://x.ai/news/designing-grok-bot):
simple geometric avatars, expressive eyes, and avatar motion that carries state.
`Thinking` becomes three animated dots and `Blocked` becomes an exclamation;
ordinary character moods keep the recognizable orb-and-eye-pair anatomy.
The shared default uses a warm-white orb, dark graphite eye marks, and blue only
for status accents. `Waiting` uses short resting marks, while `Sleepy` keeps the familiar face
placement, settles the body, and uses a longer blink. The Neutral pose defines
the common eye spacing and upper-right placement for every expression.

The expanded control model was also informed by two public implementations:
[ngocdevv/grok-bot-emoji](https://github.com/ngocdevv/grok-bot-emoji) separates
controlled expression selection from motion and supports reduced motion, while
[nasawz/GrokBot](https://github.com/nasawz/GrokBot) separates persistent state,
shape, gaze, and transient commands. BotUx uses its own compact geometry and
timings for M5GFX; it does not embed their vector paths or extracted assets.

## Geometry and rendering

The September 2026 polish keeps expressions in one anatomical family: small
changes in openness, gaze, and lean carry curiosity and skepticism. Joy eases
into shallow rounded arches; Wink closes one eye continuously; Alarmed shortens
and rounds the same eye marks. Dizzy uses a slow pair sway. Face placement,
body stretch, and body translation ease in time rather than jumping at a mood
change. The Thinking dots and Blocked exclamation remain discrete status glyphs.

The default orb uses scanline coverage with four vertical samples at its
boundary, and the eye capsules use a distance-based one-pixel edge. Solid
interiors use horizontal spans; only the boundary mixes RGB565 colors. No
supersampled canvas, dynamic allocation, or render-target ownership is added.
M5GFX's installed `fillSmoothCircle`/`fillSmoothRoundRect` were inspected; the
local path also supports the animated elliptical silhouette and oblique pills.
`readPixel` returns RGB565, which is used directly when overlapping eye strokes
blend. RoundedSquare, Hexagon, and the small status glyphs retain native M5GFX
primitives; those alternate silhouettes do not yet share the ellipse AA path.

The restrained expression changes draw conceptual guidance from
[FluxGarage/RoboEyes](https://github.com/FluxGarage/RoboEyes), which separates
openness, position, curiosity, and automatic blinking. The longer gaze pauses
and separation of persistent expression from transient motion also follow the
public interface described by [nasawz/GrokBot](https://github.com/nasawz/GrokBot).
[blessonism/grok-icon-study](https://github.com/blessonism/grok-icon-study) is
another public motion-engine study; it explicitly excludes third-party geometry.
These references informed behavior, not copied drawing code or assets.

For a color picker, copy `bot.style()`, update its RGB565 `bodyColor`, `eyeColor`,
`bgColor`, or `accentColor`, then call `setStyle()`. Hosts own color selection
and persistence. For leisurely expression previews, use a 650–700 ms transition
and hold each pose for several seconds. IMU inputs are filtered over 240 ms at
normal motion; tilt displacement is about 5.5% of body radius horizontally and
4.2% vertically before the user's motion amount is applied.

## Integration in a PlatformIO project

```ini
lib_extra_dirs = ../../lib   ; from either device project in this repository
lib_deps =
  m5stack/M5Unified
  m5stack/M5GFX
```

## Host preview

`tools/host-preview/render.sh` compiles the real `BotUx.cpp` against a small
SVG-emitting `M5Canvas` stand-in. It generates source-derived 40, 72, and 200 px
mood sheets plus expression and animation pickers. `animation-player.html`
plays 60 real C++-rendered frames for every choreography, the automatic
lifecycle, expression transitions, and IMU motion. Assertions cover visibly
distinct choices, direct and eased transitions, IMU response, reduced motion,
and sparse-frame blink catch-up without claiming device raster fidelity.

The preview additionally checks RGB565 edge color coverage and face placement
at 72, 200, and 310 px, and emits `transitions.svg` with consecutive Joy/Wink
transition frames. Pixel rectangles are displayed without browser interpolation.

The same continuous pill geometry also runs at 40 px, including Joy and Wink in
Core2's toolbar/settings preview. An explicitly selected `Neutral` resets the
face to its baseline proportions while retaining the mood's body rhythm;
`Auto` continues to use Sleepy/Waiting resting eyes. The preview verifies this
reopening behavior at 40, 72, and 200 px and emits `expressions-40.svg`.

A parent-run isolated StopWatch benchmark (310 px orb, existing host app) measured
30.4 FPS after AA, with 16.73 ms drawing and 14.44 ms transfer, compared with the
previous 39 FPS / 8.7 ms drawing. This is a measured quality/performance tradeoff,
not a claim that antialiasing is free. Final application timing is tracked by the
device integration validation.

## Companion semantics (September 2026)

The name defaults to `Milo`. `setName()` copies up to `kNameMax` (16) ASCII
letters, digits, spaces, hyphens and apostrophes, strips other bytes and trims
outer spaces. Null/empty input resets to Milo. The host owns persistence and its
English-only name editor; the Bot owns the bounded copy.

`Language::English` and `Language::Chinese` select static UTF-8 labels through
`moodName`, `expressionName`, and `animationName`. `describe(buffer, capacity,
language)` writes a short natural description, for example `Milo is resting`,
`Milo feels dizzy`, `Milo is thinking`, or `Milo 正在思考`. While Idle, an explicitly
selected expression supplies the description; otherwise the effective mood
(including transient reactions) takes precedence. This keeps captions readable
without listing enum labels. Menu labels still use the static name helpers.
It returns the required byte count,
always terminates a nonempty buffer, and avoids cutting UTF-8 characters. Hosts
must provide an appropriate font; BotUx does not add font assets or render this
text automatically. Names remain English in either language.

`moodCount()` = 12, `expressionCount()` = 10 (including Auto), and
`animationCount()` = 8 (including Auto). All existing enum numeric values remain
unchanged. Hosts can independently iterate all **960 combinations** in a preview
Bot. `mood()`, `expression()`, `animation()` report selections;
`effectiveMood()` / `effectiveExpression()` report mood reactions and Auto's
resolved face. Thinking and Blocked retain their intentional silhouette
replacements, so a selected expression is preserved but not visible in those
moods. Preview Bot state must not overwrite actual host status.

`presetCount()` / `preset(index)` expose eight curated combinations. Indices wrap.
`applyPreset(index)` applies one; `nextPreset()` advances and returns the index;
`randomPreset()` returns a different preset using independent deterministic RNG
(seed with `seedBlink` for device variation). `resetToIdle()` clears reactions and
restores Idle / Auto / Auto. They preserve style, name, speed and motion settings.
Hosts map A to random, B to next and double B to reset, delaying single B until
the double-click window closes.

`gazeAt(x, y, holdMs=1800)` takes normalized canvas coordinates, with negative x
left, negative y up, each clamped to [-1,1]. Map a canvas-local tap as
`2*x/width-1`, `2*y/height-1`. Only persistent Idle accepts it. The target eases in,
expires using the same `millis()` clock as `update`, and then resumes wandering;
`clearGaze()` cancels it. It does not call `poke` or change mood. Idle gaze now has
24% radius horizontal and 20% vertical travel, independent of animation motion
amount, preserving the characteristic neutral eye arrangement.

### Eye coverage and validation

Joy is now a single swept quadratic curve. Twelve small chords approximate its
centerline with under 0.04 pixel flattening error at eyeRadius 24; each pixel
receives coverage from the union distance exactly once. Endcaps remain round,
and the neutral-to-Joy control points interpolate continuously. This removes
independent capsule alpha overlap and the old three-piece geometry's bumps.
At <=48px, the curve width is bounded to retain separation between the eyes.
The curve uses 240 bytes of fixed float arrays on the stack and no frame heap
allocation or extra framebuffer.

Source-derived previews (top: Neutral to Joy; bottom: Neutral to Wink; first
six frames plus part of the seventh, 100ms apart) capture the prior implementation
and this change using the same existing host preview renderer:

![Previous eye morph](docs/eye-morph-before.png)
![Continuous eye morph](docs/eye-morph-after.png)

These are RGB565 coverage primitives rendered through the SVG host adapter,
not native device captures. Regenerate current source sheets with
`tools/host-preview/render.sh`; the durable PNGs preserve the before/after
comparison. The default rounded body and eye primitives are backed by real
RGB565 raster pixels; other shapes remain SVG approximations.

Validation on 2026-09-06: existing host checks pass; added checks exercise all 960
selector combinations, safe names, tiny UTF-8 buffers, preset transitions,
temporary tap gaze and expiry. A connected-component pixel check verifies two
continuous eye marks through 40 morph frames for all four eye styles at 40, 72,
and 200px (480 frames). It caught and fixed eyes touching at 40px.

Clang C++11 `-O2`, 500 draws, raster-backed host adapter with SVG recording
disabled: full Neutral/Joy draw at 40px **3.12/3.42 µs**, 72px **8.25/8.91 µs**,
200px **48.33/50.68 µs**. These isolate CPU/raster cost from SVG serialization and
are not ESP32 timings. The parent integration owns native frame/memory timing,
firmware builds, flashing and actual device captures.
