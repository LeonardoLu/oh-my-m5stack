# core2 — Codex Micro BLE controller (M5Stack Core2)

This app adapts the Codex Micro control model to a 320x240 touch display and
connects to the Codex desktop app through a compatible BLE HID vendor report.

## Hardware

- M5Stack Core2 v1.3, landscape ILI9342C display and FT6336U touch.
- M5GO Battery Bottom2 v1.3 replaces the stock Core2 bottom.
- Bottom2 has ten SK6812 LEDs on GPIO 25, three bytes per LED in GRB order.
- Bottom2 does not provide the stock bottom's vibration motor. Feedback uses the
  speaker and side LEDs; do not add vibration behavior or a vibration setting.

## Module layout

    core2/bot-ux-codex-core2/
      include/AudioFeedback.h    short speaker feedback
      include/BatteryDoubleTap.h battery settings-entry gesture state
      include/BottomLeds.h       Bottom2 host-lighting mirror
      include/CodexLink.h        BLE HID identity, RPC, and native events
      include/HidFraming.h       fixed-buffer report 6 framing
      include/Settings.h         NVS settings and four-page personalization UI
      src/main.cpp               touch dispatch, rendering, and diagnostics
      test/                      platform-independent framing/input checks

## Product behavior

- Advertise as Core2 Codex Micro using BLE HOGP. Codex compatibility uses VID
  0x303A, PID 0x8360, usage page 0xFF00, and report ID 6.
- A standard idle keyboard report exists only so operating systems offer BLE
  enrollment. Never send keyboard reports or substitute keyboard shortcuts for
  Codex vendor events.
- Six agent keys emit AG00..AG05. Controls emit the app defaults: Fast, Approve,
  Decline, Fork, push-to-talk, Send, encoder, and analog events.
- HOLD MIC sends Codex app press/release events. The Mac records audio; Core2
  does not capture or transmit microphone audio. Dragging off releases.
- A BLE connection is not enough to show Codex-ready. Require a valid app RPC,
  notification subscription, and a negotiated MTU large enough for report 6.
- The app supplies agent/key/ambient colors and effects. Do not invent task
  titles, agent statuses, or successful host actions.
- The top bar uses separate Bluetooth and app-ready icons. The battery is the
  settings entry: two valid taps within 420 ms open settings; one tap is inert.
- Settings provide continuous RGB editing for the bot body, eyes, and accent,
  plus style, expression, and animation previews. Save valid edits in NVS.
- Reject bootloader/firmware operations; incompatible hardware firmware must
  never be accepted by the Core2 emulator.

## Engineering rules

- Controls activate on a valid release in the original target. All primary touch
  targets are at least 40 px high. Push-to-talk also releases on drag-off.
- Bottom pagers use their full left and right halves as touch targets. Serial
  capture selectors are 0 Agents, 1 Control, 2 General, 3 Bot, 4 Color, 5 Motion.
- Avoid allocation in update/draw and BLE framing paths. Cache static UI and
  bound frame pacing so BLE callbacks stay responsive.
- BLE callbacks only latch connection/MTU state. The main loop owns reconnect,
  RPC state, and pending event cleanup.
- Check both sprite allocations before binding or drawing BotUx.
- Settings persist with Preferences; keep display, audio, theme, bot animation,
  IMU motion, reduced motion, and Bottom2 LED brightness.
- Keep UI text colors independent of BotUx eyeColor; the shared bot's default
  eyes are intentionally dark.
- Prefer host tests, PlatformIO builds, serial metrics/capture, hidapi
  enumeration, and Codex logs over manual-only acceptance.
