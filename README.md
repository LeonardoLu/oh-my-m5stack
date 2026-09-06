# oh-my-m5stack

M5Stack companion firmware built around one shared **bot-ux** animation component,
with a Grok Bot-inspired orb and state-driven motion.

| App | Path | Device | What it does |
|---|---|---|---|
| **watch** | `stopwatch/bot-ux-watch/` | M5Stack StopWatch (ESP32-S3) | RTC watch, date, battery/charging, companion and settings; no stopwatch |
| **codex** | `core2/bot-ux-codex-core2/` | M5Stack Core2 + M5GO Battery Bottom2 v1.3 | Codex Micro-compatible Bluetooth HID controller with host RGB feedback |
| **bot-ux** | `lib/bot-ux/` | (shared) | The bot animation component — expressions, behaviors, eased transitions, IMU motion and personalization |

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

Tap the bot or press A to interact. Tap the time to toggle seconds. Swipe left/right
or press B to cycle expressions.
Hold A and B together for three seconds to open settings. Long-press the bot for
its personalization page. Hold B to dim the face; the next touch/button press
wakes it without activating a control.

Swipe down from the top edge for a compact battery panel with charging indication.
Settings include time, date, 12/24-hour format, expression, appearance, animation,
wrist motion, brightness and sound.
Large touch rows scroll through nested settings. Editors have explicit Back/Done
controls and a live HSV body-color picker. Year, month, day, weekday and time share
one face region. Time and date use the hardware RTC; other preferences use NVS.
Auto expression slowly shuffles calm moods, while interaction delays the next
change. Filtered wrist motion adds subtle movement with a cooldown on shake reactions.

## The bot

`bot-ux` renders into a caller-provided `M5Canvas` sprite and never owns the
display. Hosts control mood, talking, interaction, and appearance. Animation uses
elapsed time and fixed state rather than frame-count timing. See
[the component guide](lib/bot-ux/README.md).

## Core2 controller

The 320×240 command deck exposes six agent keys, Fast/Approve/Decline/Fork/Mic/Send,
encoder navigation, encoder press, and a touch joystick. These are the installed
Codex app's default mappings; the app can remap them. Pair **Core2 Codex Micro** in
macOS Bluetooth settings, then enable/connect Micro in Codex. Core2's USB connector
is a serial bridge, so host HID communication uses Bluetooth.

On macOS, Codex needs Input Monitoring permission to open this composite HID
interface. After enabling it in Privacy & Security, fully quit and reopen Codex.
If the device is paired but stays at `BLE`, check this permission and restart;
the green app-ready icon indicates that the RPC transport has initialized.

The header uses separate Bluetooth and app-ready icons to distinguish pairing
from an initialized RPC transport. Agent colors and the ten Bottom2 LEDs reflect host lighting data;
there are no invented task names or simulated progress. End-to-end connection
validation is tracked in [the iteration record](specs/interaction-validation.md).

Holding MIC sends push-to-talk press/release events; the Mac captures the audio.
Releasing or dragging off stops the hold, and the host decides whether to submit.
Other buttons activate on release in the original target. The bottom left/right
halves switch pages around a compact page count. Double-tap the battery to open
settings for brightness, audio, theme, animation, motion, reduced motion and LED
brightness. Bot personalization includes live RGB sliders for body, eyes and
accent; valid releases save in NVS and dragging outside cancels the color edit.

Bottom2 uses ten SK6812 LEDs on GPIO25 and replaces the stock Core2 bottom. Screen,
LEDs and optional sound provide feedback. Official Micro firmware updates are
unsupported because Core2 is different hardware.

## Host checks and previews

These checks require a C++11 compiler, without an attached device:

```bash
mkdir -p tmp
for watch_test in test_calendar_math test_input_semantics test_timed_state test_watch_interaction; do
  c++ -std=c++11 -Wall -Wextra -Werror \
    -Istopwatch/bot-ux-watch/include \
    "stopwatch/bot-ux-watch/test/$watch_test.cpp" -o "tmp/$watch_test"
  "./tmp/$watch_test"
done
c++ -std=c++11 -Wall -Wextra -Werror \
  -Icore2/bot-ux-codex-core2/include \
  core2/bot-ux-codex-core2/src/HidFraming.cpp \
  core2/bot-ux-codex-core2/test/hid_framing_test.cpp -o tmp/hid-framing-test
./tmp/hid-framing-test
c++ -std=c++11 -Wall -Wextra -Werror \
  -Icore2/bot-ux-codex-core2/include \
  core2/bot-ux-codex-core2/src/AnalogInput.cpp \
  core2/bot-ux-codex-core2/test/analog_input_test.cpp -o tmp/analog-input-test
./tmp/analog-input-test
c++ -std=c++11 -Wall -Wextra -Werror \
  -Icore2/bot-ux-codex-core2/include \
  core2/bot-ux-codex-core2/test/battery_double_tap_test.cpp -o tmp/battery-double-tap-test
./tmp/battery-double-tap-test
sh lib/bot-ux/tools/host-preview/render.sh
```

The BotUx preview compiles the real renderer against an SVG drawing stand-in.
Generated files stay under `tmp/`. They help inspect geometry and state behavior;
they do not measure physical display performance or reproduce M5GFX text exactly.

## Design docs

[Task specification](specs/start-up.md),
[implementation decisions and reference sources](specs/implementation-notes.md),
[baseline validation](specs/validation.md),
[HID iteration requirements](specs/interaction-iteration.md),
[HID validation](specs/interaction-validation.md),
[current UX requirements](specs/ux-polish.md),
[current UX validation](specs/ux-polish-validation.md), and the `AGENTS.md` files document the durable contracts. `tmp/` contains ignored
research, generated previews and validation logs.

## License

MIT
