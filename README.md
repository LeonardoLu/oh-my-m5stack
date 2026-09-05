# oh-my-m5stack

M5Stack companion firmware built around one shared **bot-ux** animation component — a
controllable "grok bot" face — integrated into two device apps.

| App | Path | Device | What it does |
|---|---|---|---|
| **watch** | `stopwatch/bot-ux-watch/` | M5StickC Plus2 | Watch face + stopwatch + settings, with the bot as the face and state readout |
| **codex** | `core2/bot-ux-codex-core2/` | M5Stack Core2 | Touch "codex" prompt app with a custom on-screen keyboard + chat + settings |
| **bot-ux** | `lib/bot-ux/` | (shared) | The bot animation component — moods, blink, drift, breathing, reactions, personalization |

## Layout

```
lib/bot-ux/                      shared bot component (PlatformIO library)
stopwatch/bot-ux-watch/      watch app (PlatformIO project)
core2/bot-ux-codex-core2/    core2 app (PlatformIO project)
specs/                       task specs
tmp/                         scratch / research notes (gitignored)
```

## Build

Each app is a PlatformIO project. The shared `bot-ux` library is discovered via
`lib_extra_dirs = ../../lib`:

```bash
cd stopwatch/bot-ux-watch   # or core2/bot-ux-codex-core2
pio run                     # compile
pio run -t upload           # flash over USB
```

Dependencies: `m5stack/M5Unified`, `m5stack/M5GFX`; platform `espressif32@6.13.0`.

## The bot

`bot-ux` renders into a caller-provided `M5Canvas` sprite — it never touches the
display. Moods (`Idle/Listening/Thinking/Speaking/Happy/Sad/Sleepy/Surprised`), a
seedable blink/drift system, a `poke()` surprise→happy reaction, and a `Style` for
personalization (colors, eye/body shape, size). See `lib/bot-ux/README.md`.

## Design docs

`tmp/ux-design.md` and `tmp/architecture.md` hold the interaction and architecture
specs (authored during development); the durable contracts are `AGENTS.md` at each
level.

## License

MIT
