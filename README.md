# oh-my-m5stack

M5Stack companion firmware built around one shared **bot-ux** animation component,
with a Grok Bot-inspired orb and state-driven motion.

| App | Path | Device | What it does |
|---|---|---|---|
| **watch** | `stopwatch/bot-ux-watch/` | M5Stack StopWatch (ESP32-S3) | RTC watch, date, battery/charging, companion and settings; no stopwatch |
| **codex** | `core2/bot-ux-codex-core2/` | M5Stack Core2 + M5GO Battery Bottom2 v1.3 | Codex Micro-compatible Bluetooth HID controller with host RGB feedback |
| **ux-components** | `lib/ux-components/` | (shared) | Native text/shapes, pointer gestures, scrolling, keyboard and synthesized/PCM sound; products link selectively |
| **bot-ux** | `lib/bot-ux/` | (shared) | The bot animation component — expressions, behaviors, eased transitions, IMU motion and personalization |

## Layout

```
lib/bot-ux/                 shared bot component (PlatformIO library)
lib/ux-components/          reusable native UI products and subset fonts
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

Press A to choose a random companion preset. B advances to the next preset;
double-click B within 320 ms to return to idle. While idle, tapping the screen
briefly moves the bot's gaze toward the tap. Hold A+B together for three seconds
to open settings; long-press the bot for personalization. Hold B to dim the face;
the next touch/button press wakes it without activating a control.

Swipe down from the top edge for the compact battery/charging panel. Settings
lists follow the finger with continuous pixel scrolling and inertia. Editors
have Back/Done controls, native antialiased text and a live HSV body-color picker.
Settings capture one pointer target, tolerate small finger drift and slow holds,
and show a pressed state; a scroll gesture never also clicks a row.
Time and `yyyy/mm/dd {weekday}` share one region. A hideable Bot description
occupies the opposite region; Layout swaps the two. Time/date use the RTC;
preferences use NVS. Auto expression slowly shuffles calm moods, while interaction
delays the next change. Filtered wrist motion adds subtle movement with a cooldown
on shake reactions.

The companion is **Milo** by default. Both devices support a 16-character English
name, English/Chinese descriptions, and independent live selectors for all
12 moods × 10 expressions × 8 animations (960 combinations). Preview choices do
not overwrite the persistent expression/action or Core2 host state. Gaze can be
Auto, Center, Left, Right, Up or Down. Temporary screen-directed gaze returns to
that preference; normal presets retain continuous motion.

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
from an initialized RPC transport. Status glyphs are right-aligned, with the
battery number inside its icon. Agent cards use the exact host color, a corner
number and the corresponding lighting status. The selected Bot follows that
slot. Blue means Working, orange Needs input, green New reply, red Error and
white Idle. Green does not establish completion; unknown signals stay Unknown.
Meaningful transitions trigger a bounded highlight and optional sound, with silent
initial/reconnect baselines. See [the signal contract](specs/agent-signal-contract.md). End-to-end connection
validation is tracked in [the iteration record](specs/interaction-validation.md).

Holding MIC sends push-to-talk press/release events; the Mac captures the audio.
Releasing or dragging off stops the hold, and the host decides whether to submit.
Other buttons activate on release in the original target. The bottom left/right
halves switch pages around a compact page count. Double-tap the battery to open
settings for brightness, audio, theme, animation, motion, reduced motion and LED
brightness. The Lights page offers Off, Host and Alive modes plus transition
notifications. Alive preserves host hues with gentle breathing, a traveling
highlight and selection feedback; Reduced Motion keeps it static. Bot
personalization includes naming, language, complete combination preview and live RGB sliders for body, eyes and
accent; valid releases save in NVS and dragging outside cancels the color edit.

Bottom2 uses ten SK6812 LEDs on GPIO25 and replaces the stock Core2 bottom. Screen,
LEDs and optional sound provide feedback. Official Micro firmware updates are
unsupported because Core2 is different hardware.

Both devices share 16 synthesized cues with six timbres, envelopes, glides and
scales; optional signed/unsigned 8-bit PCM is a separate product. A fixed-buffer
sender isolates the SDK playback queue from the UI loop. Sound preferences apply
to new cues, and queued audio drains smoothly. See [the sound guide](lib/ux-components/SOUND.md).

## Host checks and previews

These checks require a C++11 compiler, without an attached device:

```bash
sh tools/check_host.sh
```

The suite runs 15 standalone C++ contracts plus the real BotUx renderer checks.
It covers input timing, continuous scrolling, native font/shape coverage, naming,
960 combinations at multiple late time windows, gaze and dot antialiasing, HID
framing, exact host status projection, LED envelopes, sound waveforms/PCM and
playback-thread stalls.
Artifacts stay under `tmp/host-checks/`. Host raster timings are not device FPS;
[native firmware acceptance](specs/interaction-dynamics-validation.md) records that separately.

## Design docs

[Task specification](specs/start-up.md),
[implementation decisions and reference sources](specs/implementation-notes.md),
[baseline validation](specs/validation.md),
[HID iteration requirements](specs/interaction-iteration.md),
[HID validation](specs/interaction-validation.md),
[previous UX validation](specs/ux-polish-validation.md),
[previous shared UX requirements](specs/ux-components-iteration.md),
[current requirements](specs/interaction-dynamics.md),
[shared UX library](specs/ux-components-library.md),
[previous shared UX validation](specs/ux-components-validation.md),
[current validation](specs/interaction-dynamics-validation.md), and the `AGENTS.md` files document the durable contracts. `tmp/` contains ignored
research, generated previews and validation logs.

## License

MIT
