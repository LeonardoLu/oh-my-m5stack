# bot-ux

A small, controllable **grok bot** animation component for M5Stack devices.

It draws a character (face, body, antenna, status overlays) into a
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
  bot.setSignal(3);
  bot.setTime(9, 41, 0, false);
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

- **Moods** — `Idle`, `Listening`, `Thinking`, `Speaking`, `Happy`, `Sad`,
  `Sleepy`, `Surprised` via `setMood()`.
- **Personalization** — `Style` holds colors, eye style (round/oval/square/googly),
  body style (none/round/rounded-square/hexagon), eye scale, blink rate.
- **Status** — `setBattery`, `setSignal`, `setTime`, `setLabel`.
- **Interaction** — `poke()` (transient surprise→happy), `setTalking(bool)`.
- **Frame loop** — `update(nowMs)` then `draw()`.

The component renders into whatever canvas you give it and scales itself to
that canvas, so a bot can be full-screen (watch) or a small tile (chat header).

## Integration in a PlatformIO project

```ini
lib_extra_dirs = ../..   ; so PlatformIO discovers bot-ux at the repo root
lib_deps =
  m5stack/M5Unified
  m5stack/M5GFX
```
