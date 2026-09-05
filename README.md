# oh-my-m5stack

M5Stack companion firmware built around one shared **bot-ux** animation component,
with a Grok Bot-inspired orb and state-driven motion.

| App | Path | Device | What it does |
|---|---|---|---|
| **watch** | `stopwatch/bot-ux-watch/` | M5Stack StopWatch (ESP32-S3) | RTC watch, date, battery/charging, companion and settings; no stopwatch |
| **codex** | `core2/bot-ux-codex-core2/` | M5Stack Core2 + M5GO Battery Bottom2 v1.3 | Codex Micro-inspired agent command deck with local simulation and RGB status feedback |
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

Run from the repository root:

```bash
pio run -d stopwatch/bot-ux-watch
pio run -d core2/bot-ux-codex-core2
```

To install on a device, identify its port with `pio device list`, then use the
matching project and an explicit port:

```bash
pio run -d stopwatch/bot-ux-watch -t upload --upload-port <StopWatch-port>
pio run -d core2/bot-ux-codex-core2 -t upload --upload-port <Core2-port>
```

Dependencies: `m5stack/M5Unified`, `m5stack/M5GFX`; platform `espressif32@6.13.0`.
Library revisions are pinned in each app's `platformio.ini`; the StopWatch also
uses the M5PM1/M5IOE1 drivers and OPI PSRAM configuration.

## Watch controls

Tap the bot or press A to interact. Tap the time or press B to toggle seconds.
Tap SET, swipe up, or hold A for settings. Swipe down or hold B to dim the face;
the next touch/button press wakes it without activating a control.

Settings include time, date, 12/24-hour format, appearance, brightness and sound.
Editors have explicit Back/Done controls. Time and date use the hardware RTC;
other preferences use NVS.

## The bot

`bot-ux` renders into a caller-provided `M5Canvas` sprite and never owns the
display. Hosts control mood, talking, interaction, and appearance. Animation uses
elapsed time and fixed state rather than frame-count timing. See
[the component guide](lib/bot-ux/README.md).

## Core2 simulation

The command deck adapts Codex Micro's agent keys, workflow launcher, command keys,
and reasoning control to a 320×240 touch screen. Its agents and task progress are
local demonstrations. It does not connect to Codex, send keyboard shortcuts to a
computer, execute code, or record/transmit voice.

The ten Bottom2 side LEDs use GPIO25. Bottom2 replaces the stock Core2 bottom;
feedback uses screen, LEDs and optional sound. The firmware does not rely on the
stock bottom's vibration motor.

On AGENTS, tap a key to select it. CONTROL exposes workflows, approval commands,
New, a hold-to-talk simulation, and Low/Medium/High reasoning. Drag off a pressed
control to cancel. The three bottom touch buttons select previous, switch page,
and select next. SET contains display, audio, theme, simulation speed, reduced
motion and LED brightness.

## Host checks and previews

These checks require a C++11 compiler, without an attached device:

```bash
mkdir -p tmp
for watch_test in test_calendar_math test_input_semantics test_timed_state; do
  c++ -std=c++11 -Wall -Wextra -Werror \
    -Istopwatch/bot-ux-watch/include \
    "stopwatch/bot-ux-watch/test/$watch_test.cpp" -o "tmp/$watch_test"
  "./tmp/$watch_test"
done
c++ -std=c++11 -Wall -Wextra -Werror \
  -Icore2/bot-ux-codex-core2/include \
  core2/bot-ux-codex-core2/src/AgentModel.cpp \
  core2/bot-ux-codex-core2/test/agent_model_test.cpp -o tmp/agent-model-test
./tmp/agent-model-test
sh lib/bot-ux/tools/host-preview/render.sh
```

The BotUx preview compiles the real renderer against an SVG drawing stand-in.
Generated files stay under `tmp/`. They help inspect geometry and state behavior;
they do not measure physical display performance or reproduce M5GFX text exactly.

## Design docs

[Task specification](specs/start-up.md),
[implementation decisions and reference sources](specs/implementation-notes.md),
[validation record](specs/validation.md), and the `AGENTS.md` files document the durable contracts. `tmp/` contains ignored
research, generated previews and validation logs.

## License

MIT
