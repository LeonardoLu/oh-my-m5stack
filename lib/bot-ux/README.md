# bot-ux

A small, controllable Grok-inspired bot animation component for M5Stack devices.

It draws a minimal orb and two expressive eye marks into a
caller-provided [`M5Canvas`](https://docs.m5stack.com/en/arduino/m5stack/m5gfx)
sprite. The component never touches the display — the host creates the sprite,
drives `update()`/`draw()` each frame, and pushes the sprite when convenient.
That keeps it embeddable into both a watch (StickC Plus2) and a touch tablet
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
- **Status** — `setBattery`, `setSignal`, `setTime`, `setLabel`.
- **Interaction** — `poke()` (quick surprise→happy), `setTalking(bool)` to
  drive the speaking pulse.
- **Frame loop** — `update(nowMs)` then `draw()`.

The component renders into whatever canvas you give it and scales itself to
that canvas, so a bot can be full-screen (watch) or a small tile (chat header).
Its draw path uses fixed stack locals and M5GFX primitives, with no per-frame
heap allocation.

The visual system follows xAI's published [Grok Bot design notes](https://x.ai/news/designing-grok-bot):
simple geometric avatars, expressive eyes, and avatar motion that carries state.
`Thinking` becomes three animated dots and `Blocked` becomes an exclamation;
ordinary character moods keep the recognizable orb-and-eye-pair anatomy.

## Integration in a PlatformIO project

```ini
lib_extra_dirs = ../..   ; so PlatformIO discovers bot-ux at the repo root
lib_deps =
  m5stack/M5Unified
  m5stack/M5GFX
```

## Host preview

`tools/host-preview/render.sh` compiles the real `BotUx.cpp` against a small
SVG-emitting `M5Canvas` stand-in. It generates source-derived 40, 72, and 200 px
mood contact sheets for layout review without claiming device raster fidelity.
