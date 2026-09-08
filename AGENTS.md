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
wiki/bot-ux/                 bilingual illustrated catalog with native animation captures
tmp/                         scratch / research notes (gitignored)
```

## Tech stack

- PlatformIO + Arduino framework, `espressif32@6.13.0` platform.
- Libraries: `m5stack/M5Unified`, `m5stack/M5GFX`.
- `bot-ux` is a PlatformIO library; device apps discover it via `lib_extra_dirs = ../../lib`.

## BotUx contract (source of truth: `lib/bot-ux/src/BotUx.h`)

- `begin(M5Canvas*)` — bind a sprite, never owns the display.
- `setStyle(Style)` — personalization (colors, eye/body style, sizes).
- `setMood(Mood)` — persistent mood; 14 current values including append-only Asleep and LookingAround.
  Use header counts/names: 14 moods × 10 expressions × 8 animations = 1,120 combinations.
- `setTalking(bool)`, `setBattery(pct)`, `setSignal(bars)`, `setTime(h,m,s,pm)`, `setLabel(text)`.
- `setGazeDirection(GazeDirection)` — Auto plus nine explicit directions (Center, L/R/U/D, four diagonals);
  temporary `gazeAt` overrides expire.
- `setFaceSide(FaceSide)` — Auto plus fixed Left/Right mirroring; Idle defaults to UpRight and LookingAround owns autonomous gaze.
- `poke()` — transient surprise→happy reaction.
- Naming, bilingual `describe`, curated presets and temporary `gazeAt` are additive APIs.
  Counts/name helpers drive independent device combination previews.
- `update(nowMs)` + `draw()` — host drives the frame loop.

## Working rules

- Match existing code style; keep comments purposeful.
- The user's current goal, `specs/watch-gaze-experience.md`,
  `specs/watch-ambient-experience.md`, and `specs/orb-motion-experience.md`
  supersede older touch/gaze, vocabulary, and app notes. Core2 now targets a
  real Codex Micro-compatible Bluetooth HID link;
  distinguish pairing, app handshake, host feedback, and local animation previews.
- Keep the bilingual `wiki/bot-ux/intro.md` and standalone `intro.html` catalog
  current when changing moods, expressions or animation semantics; regenerate
  examples through the native host renderer rather than substituting CSS motion.
- Keep durable implementation and validation notes in `specs/`; `tmp/` is scratch,
  never the sole source of an implementation contract.
- Do not over-engineer edge cases (see `specs/start-up.md`).
- Commit meaningful units; work on `main` or a topic branch, never leave `tmp/` tracked.
- Prefer builds, automated checks, and static review over acceptance requiring a
  person. Connected serial ports alone do not establish hardware acceptance.
