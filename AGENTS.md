# oh-my-m5stack — Repository Guide

M5Stack companion firmware. One shared **bot-ux** animation component ("grok bot"),
integrated into two device apps:

- `stopwatch/bot-ux-watch` — clock companion on M5Stack StopWatch (ESP32-S3); no stopwatch feature
- `core2/bot-ux-codex-core2` — Codex Micro-inspired touch command deck on M5Stack Core2

## Layout

```
lib/bot-ux/                      shared bot animation component (PlatformIO library)
  src/BotUx.{h,cpp}          the component: face, moods, animation
  examples/standalone/       minimal demo
lib/ux-components/               selective native text, shapes, pointer, keyboard and sound products
stopwatch/bot-ux-watch/      watch app (PlatformIO project)
core2/bot-ux-codex-core2/    core2 app (PlatformIO project)
specs/                       task specs (source of truth)
tmp/                         scratch / research notes (gitignored)
```

## Tech stack

- PlatformIO + Arduino framework, `espressif32@6.13.0` platform.
- Libraries: `m5stack/M5Unified`, `m5stack/M5GFX`.
- `bot-ux` is a PlatformIO library; device apps discover it via `lib_extra_dirs = ../../lib`.

## BotUx contract (source of truth: `lib/bot-ux/src/BotUx.h`)

- `begin(M5Canvas*)` — bind a sprite, never owns the display.
- `setStyle(Style)` — personalization (colors, eye/body style, sizes).
- `setMood(Mood)` — persistent mood; use header enum/count helpers for the complete current set.
- `setTalking(bool)`, `setBattery(pct)`, `setSignal(bars)`, `setTime(h,m,s,pm)`, `setLabel(text)`.
- `setGazeDirection(GazeDirection)` — Auto/Center/Left/Right/Up/Down, temporary `gazeAt` overrides expire.
- `poke()` — transient surprise→happy reaction.
- Naming, bilingual `describe`, curated presets and temporary `gazeAt` are additive APIs.
  Counts/name helpers drive independent device combination previews.
- `update(nowMs)` + `draw()` — host drives the frame loop.

## Working rules

- Match existing code style; keep comments purposeful.
- The user's current goal and `specs/touch-gaze-iteration.md` supersede older
  app notes. Core2 now targets a real Codex Micro-compatible Bluetooth HID link;
  distinguish pairing, app handshake, host feedback, and local animation previews.
- Keep durable implementation and validation notes in `specs/`; `tmp/` is scratch,
  never the sole source of an implementation contract.
- Do not over-engineer edge cases (see `specs/start-up.md`).
- Commit meaningful units; work on `main` or a topic branch, never leave `tmp/` tracked.
- Prefer builds, automated checks, and static review over acceptance requiring a
  person. Connected serial ports alone do not establish hardware acceptance.
