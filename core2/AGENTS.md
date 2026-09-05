# core2 — codex keyboard app (M5Stack Core2)

A touch-first "codex" prompt app that hosts the shared `bot-ux` bot and a custom
on-screen keyboard.

## Hardware

- Device: M5Stack Core2, 320×240 landscape ILI9342C + FT6336U capacitive touch, RGB565.
  PlatformIO `board = m5stack-core2` (espressif32 platform), AXP192 PMU.
- Haptics: `M5.Power.setVibration(level)` (motor) + `M5.Speaker.tone()` (I2S speaker).
- Touch via `M5.Touch.getDetail(0)` (`.x/.y/.wasPressed()/…`); coords are already
  rotation-mapped — do not remap manually. There is no `getTouchPoint()`/`isPressed()`.
- 8 MB PSRAM — the 320×240@16bpp sprite (153 KB) is trivial.

## Module layout

```
core2/bot-ux-codex-core2/
  platformio.ini
  include/CodexKeyboard.h   # layout, hit-test, render, key→char
  include/ChatView.h        # transcript + bot mood wiring
  include/Settings.h        # prefs (keyboard layout, brightness, haptics, theme)
  include/Haptics.h         # beeps + vibration
  src/main.cpp              # touch dispatch + frame loop
  src/CodexKeyboard.cpp
  src/ChatView.cpp
  src/Settings.cpp
  src/Haptics.cpp
```

## Integration with bot-ux

Same `M5Canvas` + `bot.update(millis())` + `bot.draw()` + `pushSprite` loop as the
watch. The bot shrinks to a ~60 px tile on the left during typing and is full-screen
when idle. Use `bot.metrics()` to find its bounds for touch hit-testing.

## Interaction (full spec in `tmp/ux-design.md` PART C)

- Prompt loop: tap prompt → Listening → type → SEND → Thinking (…) → Speaking
  (canned reply types out) → Idle.
- Keyboard: 3-row compact QWERTY + command-chip row (`what time`, `joke`, `hello`)
  + SEND. `⇧` toggles case, `123` toggles symbol layer.
- Bot gestures: tap = `poke()`, swipe L/R = cycle theme, swipe up = Settings,
  long press = nap (Sleepy).
- Canned replies matched by substring; default → "interesting — tell me more".

## Rules

- Match existing code style; no per-frame heap allocation.
- Touch dispatch + frame loop live in `main.cpp`; modules are plain classes.
- Persist settings with `Preferences` (NVS). Do not over-engineer edge cases.
- See `tmp/architecture.md` §3b/§5 and `tmp/ux-design.md` PART C for the full design.
