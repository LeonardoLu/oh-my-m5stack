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
- The main UI separately derives a **fresh reply attraction** only from a
  deduplicated Working → NewReply event. This is the complete-like presentation
  requested by the product, but its protocol name remains `NewReply`: green does
  not prove that a task is semantically done. It remains active for up to 30
  seconds, is dismissed by screen or physical-button interaction, and is cleared
  sooner if the slot stops reporting NewReply or the app-ready link is lost.
  Persistent green frames cannot renew the deadline, and reconnect baselines do
  not create one. The active mask is passed into `update`; BottomLeds does not
  infer it from RGB values. The card uses a static brightened fill and strong
  outline while active; only the 20 FPS LED renderer animates for the full window,
  avoiding a 30-second full-screen redraw loop.
- `control(color, nowMs)`: one 900 ms traveling response to a locally sent
  control. The supplied semantic control color may fill otherwise unused ambient
  pixels in Alive; active host zones retain their own hue.
- `hold(color, active, nowMs)`: a breathing ambient response while a real local
  press is held. Releasing the hold finishes with the same bounded control flow.
  It describes only the known press lifecycle and does not infer a host toggle.
- `update(lighting, nowMs, reducedMotion, connected, selectedAgent = 0)` remains
  source-compatible with old callers. The connection argument should represent
  the app-ready connection used for the UI's lighting mirror.

## Motion language

Alive uses a cosine breathing envelope over 5.4 seconds and a broad traveling
brightness highlight over 8 seconds. Active host zones now occupy roughly
150–255 of the per-zone gain range before the global strip cap, instead of the
former 96–239 range, so known colors remain prominent through Bottom2's squared
brightness curve. The disconnected cool blue-violet presence
flows over 6.8 seconds so it remains visible at the default hardware brightness
without resembling an agent status. Selection adds a small steady emphasis;
interaction and notification feedback have smooth sine-squared envelopes.
Colors are scaled by a single brightness factor per pixel, so a working blue,
input-needed orange, or unread green stays the host's hue. Local effects do not
turn status colors white or invent a completion color. Values never exceed the
zone's host brightness before the existing global user brightness cap is applied.
During fresh-reply attraction, the green slot receives a quicker 1.2 second
breathing emphasis and unused ambient pixels carry a traveling copy of that
known host hue. Idle remains the host's steady white state, visually distinct
from this bounded NewReply notification.

Inactive agent slots stay black. If the connected host has no active ambient
zone, only the four ambient LEDs can show a quiet neutral device presence or a
known local control response. Disconnected Alive uses a bounded cool blue-violet
breath and flow on the strip; this
indicates a powered controller waiting for a link, not an active agent. Thus
Alive is an explicit local presence mode and may glow while host lighting is off;
Host mode is available when the user wants only the existing host mirror.

Reduced Motion fixes the breathing/highlight level, keeps a static selected-slot
emphasis, and suppresses the moving interaction and notification envelopes. A
fresh reply keeps a stronger static emphasis so it remains perceivable without
motion.
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
visible bounded disconnected presence and spatial flow, local hold emphasis,
notification/sweep expiry, time wrap, and bounded changes between 50 ms samples
(including simultaneous feedback).
`test/fresh_reply_attention_test.cpp` checks event-only start, exact 30 second
expiry, wraparound, interaction dismissal, current-state invalidation, filtering
of NeedsInput, and silent reconnect baselines.
The root integration owns firmware build and actual Bottom2 hardware acceptance.

## Audio and theme feedback

Core2 sets M5Unified's speaker master from its default 64 to 128. The pinned
Core2 configuration uses magnification 16, the reserved channel remains at 255,
and the source is mono. After M5Unified's rate normalization, mono duplication
and final 8-bit shift, the worst-case single-channel multiplier is
`2 * 16 * 128^2 * 255^2 / 2^36 = 0.4961`. The shared synth is limited to
±15,000, so its theoretical app-only output is bounded near ±7,442 before the
int16 clamp. Linear 16 kHz to 48 kHz interpolation cannot exceed its endpoints.
This is software headroom evidence; it is not an acoustic loudness, distortion,
or speaker-safety measurement.

The six synth steps are 0, 16, 30, 45, 96 and 255. Because the speaker master is
squared, steps 1–3 preserve their former effective software drive while step 5
has four times the former maximum sample amplitude. Changing Volume first applies
the new synth gain and mute state, then plays Select at that gain. Step zero and
Audio Feedback Off stay silent. Paper, Warm and Dark palettes use stronger
surface separation, outlines, text contrast and more saturated blue, vermilion
and cyan-blue accents respectively.

Native settings diagnostics on `/dev/cu.usbserial-5C9A0591461` cycled all six
levels from the saved level 5 and observed gains `0,16,30,45,96,255` with
speaker master 128. Each nonzero change reported `busy=1` after applying its new
gain; zero reported `busy=0`. While muted, all six changes stayed `busy=0`, then
the script restored audio, volume and theme preferences. Eight maximum-level
Confirm cues completed with `sound_fail=0`; the following steady sample reported
29.87 FPS, 61,784 B heap, 4,030,639 B PSRAM, app-ready MTU 67 and no sender
failure. This is runtime transport evidence, not acoustic measurement. Native
320×240 General, Paper, Warm and Dark canvases under `tmp/core-iteration/` were
inspected for separation, text readability and saturated accents; they are
firmware framebuffer captures rather than panel photographs.
