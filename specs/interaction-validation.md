# September 6 interaction iteration — validation

Status: implementation and on-device rendering checks are substantially complete;
**Codex end-to-end HID connection is still under validation**. Bluetooth discovery
alone is not proof of app interoperability. See [requirements](interaction-iteration.md).

## Verified implementation

- Shared BotUx has 10 selectable expression modes and eight animation modes,
  eased switching, motion input, shake reaction, amplitude/speed controls and
  reduced motion. Existing moods remain available. The host preview compiles the
  actual renderer, including 60-frame sequences for each animation/transition.
- StopWatch has a centered 310×310 companion sprite (about 242 px orb diameter),
  native FreeSans clock/settings text, direct expression switching, animation and
  wrist settings, battery overlay and automatic charging acknowledgment. It has
  no stopwatch. Preferences persist in NVS; time/date use the hardware RTC.
- Core2 replaces the local task simulation with Bluetooth HID report 6, bounded
  JSON framing, real Micro input notifications and host RGB feedback. Host data
  supplies colors, not task titles. Push to talk controls Mac audio capture.
- Independent cross review covered watch gesture origin/cancellation, settings
  rollback, low-battery/doze expression precedence, clipping, and reduced motion.
  Core2 review covered fragmentation, no-newline requests, exact 61-byte chunks,
  malformed recovery, MTU/CCCD readiness, and callback/main-loop ownership.

## Checks and measured hardware performance

Watch calendar/input/timed-state tests, Core2 HID framing/analog-direction tests, BotUx renderer
regressions, and both PlatformIO builds passed. C++ host checks use warnings as
errors. Temporary diagnostics and reference extraction stay in ignored `tmp/`.

| Device | Previous measured FPS | New measured FPS | Evidence |
| --- | ---: | ---: | --- |
| StopWatch | 17.79 | 39.0 | Five-second physical-device timing windows |
| Core2 | 18.11 | 30.30 | Final candidate, Bluetooth advertising |

StopWatch steady averages: update 0.208 ms, drawing 8.7 ms, partial display push
14.464 ms, maximum frame 25.7 ms. The orb's visible area is approximately 2.25×
the prior size. Free internal heap was 288,412 bytes, largest block 253,940 bytes,
and free PSRAM 7,321,435 bytes. Baseline full-screen push averaged 31.226 ms.

Native framebuffer captures were inspected at 466×466 for face, settings,
expression, appearance and motion. An arrow-pill radius artifact found in the
capture was fixed and recaptured. These are actual M5GFX font pixels from the
firmware canvas, not panel photographs; they establish raster layout, not outdoor
readability or physical panel color accuracy.

Core2 steady drawing maximum was 3.65 ms and partial push maximum was 5.54 ms,
with 120,480 bytes free heap and 4,030,639 bytes free PSRAM. These measurements
were collected while advertising; connected HID traffic remains to be measured.
The final native 320×240 Agents, Control and Settings page captures have correct
RGB565 byte order and readable native Font2 text. Both devices were flashed successfully with image
hash verification and booted without reset loops in the observed timing windows.

Final build sizes: StopWatch RAM 24,096 bytes, flash 596,429 bytes; Core2 RAM
46,696 bytes, flash 1,395,657 bytes.

## Connection evidence and remaining checks

- macOS Bluetooth discovers `Core2 Codex Micro` with keyboard appearance.
- The HID descriptor has an idle keyboard collection and a vendor collection;
  application commands use only the vendor report.
- DIS PnP values account for the pinned BLE library's byte-order implementation.
- Real OS HID enumeration, Codex initialization/lighting RPCs, harmless manual
  control, and disconnect/reconnect validation remain pending pairing.
- Official Micro DFU commands are rejected; its firmware cannot run on Core2.

## Design references

The StopWatch factory firmware was studied at commit
`6b4aa125288b6fe9dca661f10159f6e1e5ee785c`:
[M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo/tree/6b4aa125288b6fe9dca661f10159f6e1e5ee785c).
Its top-edge battery gesture, automatic charging view, native text, direct settings
preview, 7:1 battery filtering and partial display updates informed this iteration.

BotUx uses original geometry and animation code, informed by
[xAI's design discussion](https://x.ai/news/designing-grok-bot),
[ngocdevv's reconstruction](https://github.com/ngocdevv/grok-bot-emoji) and
[nasawz's reconstruction](https://github.com/nasawz/GrokBot). These are behavior and
design references; proprietary application code and image assets are not included.

## Reproduce device captures and HID discovery

Install `pyserial` for capture and `hidapi` for discovery in a Python environment:

```sh
python tools/capture_device_frame.py <StopWatch-port> tmp/watch.png --screen 0
python tools/capture_device_frame.py <Core2-port> tmp/core.png --boot-wait 8
python tools/micro_hid_probe.py
```

Capture is an explicit diagnostic operation and pauses the device during transfer.
`micro_hid_probe.py` enumerates by default. Its optional `--status` requests only
`device.status`; it sends no key action. A successful probe response alone does
not establish that Codex is connected.
