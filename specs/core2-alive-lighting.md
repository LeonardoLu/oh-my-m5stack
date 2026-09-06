# Core2 Bottom2 Alive lighting

The Bottom2 driver still uses GPIO 25, ten SK6812 LEDs, three-byte GRB ordering,
and M5Unified's LED strip/color packing. The pinned Arduino toolchain uses
ESP-IDF 4.4.7; M5Unified 0.2.21 leaves its legacy RMT bus init/write branches
unimplemented. BottomLeds supplies a real IDF4 bus using rmt_config,
rmt_driver_install and synchronous rmt_write_items, selecting an unoccupied
channel. It preallocates 240 data symbols plus a 280 µs reset and reports actual
initialization/write failures. IDF5 builds continue to use M5Unified's bus. Allocation occurs only in `begin()`;
frame generation uses fixed stack arrays. Normal updates are limited to 20 FPS
with a 50 ms minimum interval. Explicit Off/brightness-zero actions clear the
strip immediately instead of waiting for the next scheduled animation frame.

## Modes and API

- `setMode(0)`: **Off**, all ten LEDs black in every connection/motion state.
- `setMode(1)`: **Host**, retains the existing per-zone host lighting renderer.
  Slots map to physical LEDs 0,1,2,7,8,9; ambient maps to 3–6. Disconnected is black.
  Host solid, breath, shallow breath, snake and rainbow handling is preserved.
  Local interaction/notification effects never alter Host mode.
- `setMode(2)`: **Alive**, the default. Retains each active zone's host RGB hue,
  adds gentle brightness variation, and distinguishes the selected slot.
- `setBrightness(level)`: independent global strip limit: 0/1/2/3 maps to the
  linear output caps near 0/16/38/72 of 255. M5Unified applies the squared gain
  `(brightness+1)^2/65536`, so the adapter uses driver values 0/64/98/135.
  Full-intensity channels quantize to 0/16/38/71 after this curve.
  Passing 16/38/72 directly would severely dim the strip and quantize Alive's
  quiet presence to black at the default level. Level zero overrides every mode, including
  Alive's disconnected presence. Re-enabling brightness preserves the chosen mode.
- `interact(selected, nowMs)`: 900 ms soft traveling highlight around the chosen
  slot, useful when selecting another card or acknowledging local interaction.
- `notify(mask, nowMs)`: one 1800 ms raised-and-settled brightness pulse on the
  six-bit slot mask. The UI supplies the AgentSignal model's deduplicated mask;
  no task state or completion is inferred inside this driver. A zero mask cancels
  active notification feedback immediately.
- `update(lighting, nowMs, reducedMotion, connected, selectedAgent = 0)` remains
  source-compatible with old callers. The connection argument should represent
  the app-ready connection used for the UI's lighting mirror.

## Motion language

Alive uses a cosine breathing envelope over 5.4 seconds and a broad traveling
brightness highlight over 8 seconds. Selection adds a small steady emphasis;
interaction and notification feedback have smooth sine-squared envelopes.
Colors are scaled by a single brightness factor per pixel, so a working blue,
input-needed orange, or unread green stays the host's hue. Local effects do not
turn status colors white or invent a completion color. Values never exceed the
zone's host brightness before the existing global user brightness cap is applied.

Inactive agent slots stay black. If the connected host has no active ambient
zone, only the four ambient LEDs can show a quiet neutral device presence.
Disconnected Alive uses a low-intensity cool-gray breath on the strip; this
indicates a powered controller waiting for a link, not an active agent. Thus
Alive is an explicit local presence mode and may glow while host lighting is off;
Host mode is available when the user wants only the existing host mirror.

Reduced Motion fixes the breathing/highlight level, keeps a static selected-slot
emphasis, and suppresses the moving interaction and notification envelopes.
Disconnected presence becomes static too. No repeated blink or abrupt strong
flash is used. Switching an actual host status still changes that host color;
local animation does not conceal a status update.

## Pure host checks

`include/BottomLedFrame.h` contains RGB generation without Arduino/M5GFX or
hardware dependencies. Run:

```sh
c++ -std=c++11 -Wall -Wextra -Werror \
  -Icore2/bot-ux-codex-core2/include \
  core2/bot-ux-codex-core2/test/bottom_led_frame_test.cpp \
  -o /tmp/bottom-led-frame-test
/tmp/bottom-led-frame-test
```

Checks cover Off and brightness-zero black output, Host solid color preservation,
Host disconnection and ignoring local events, Alive time variation while retaining
host hue, static Reduced Motion and selection emphasis, inactive-slot blackness,
low-intensity disconnected neutral presence, notification/sweep expiry, time wrap,
and bounded changes between 50 ms samples (including simultaneous feedback).
The root integration owns firmware build and actual Bottom2 hardware acceptance.
