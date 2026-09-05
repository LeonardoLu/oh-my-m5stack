# oh-my-m5stack — Repository Guide

M5Stack companion firmware. One shared **bot-ux** animation component ("grok bot"),
integrated into two device apps:

- `stopwatch/bot-ux-watch` — watch/stopwatch on M5StickC Plus2
- `core2/bot-ux-codex-core2` — codex-style on-screen keyboard on M5Stack Core2

## Layout

```
bot-ux/                      shared bot animation component (PlatformIO library)
  src/BotUx.{h,cpp}          the component: face, moods, animation
  examples/standalone/       minimal demo
stopwatch/bot-ux-watch/      watch app (PlatformIO project)
core2/bot-ux-codex-core2/    core2 app (PlatformIO project)
specs/                       task specs (source of truth)
tmp/                         scratch / research notes (gitignored)
```

## Tech stack

- PlatformIO + Arduino framework, `espressif32@6.13.0` platform.
- Libraries: `m5stack/M5Unified`, `m5stack/M5GFX`.
- `bot-ux` is a PlatformIO library; device apps discover it via `lib_extra_dirs = ../..`.

## BotUx contract (source of truth: `bot-ux/src/BotUx.h`)

- `begin(M5Canvas*)` — bind a sprite, never owns the display.
- `setStyle(Style)` — personalization (colors, eye/body style, sizes).
- `setMood(Mood)` — persistent mood (Idle/Listening/Thinking/Speaking/Happy/Sad/Sleepy/Surprised).
- `setTalking(bool)`, `setBattery(pct)`, `setSignal(bars)`, `setTime(h,m,s,pm)`, `setLabel(text)`.
- `poke()` — transient surprise→happy reaction.
- `update(nowMs)` + `draw()` — host drives the frame loop.

## Working rules

- Match existing code style; keep comments purposeful.
- Do not over-engineer edge cases (see `specs/start-up.md`).
- Commit meaningful units; work on `main` or a topic branch, never leave `tmp/` tracked.
- Prefer static review over hardware/device acceptance (no device in this environment).
