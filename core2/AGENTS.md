# core2 — Codex Micro local simulation (M5Stack Core2)

This app adapts the Codex Micro control model to a 320x240 touch display. It is
an honest local simulation: it never claims to control a host, send audio, or
synchronize real Codex tasks.

## Hardware

- M5Stack Core2 v1.3, landscape ILI9342C display and FT6336U touch.
- M5GO Battery Bottom2 v1.3 replaces the stock Core2 bottom.
- Bottom2 has ten SK6812 LEDs on GPIO 25, three bytes per LED in GRB order.
- Bottom2 does not provide the stock bottom's vibration motor. Feedback uses the
  speaker and side LEDs; do not add vibration behavior or a vibration setting.

## Module layout

```
core2/bot-ux-codex-core2/
  include/AgentModel.h       fixed six-agent simulation state machine
  include/AudioFeedback.h    short speaker feedback
  include/BottomLeds.h       Bottom2 SK6812 status mirror
  include/Settings.h         NVS settings and two-page settings UI
  src/main.cpp               touch dispatch, rendering, bounded frame loop
  test/agent_model_test.cpp  platform-independent model checks
```

## Product behavior

- Six agent keys start in idle, thinking, running, waiting, done, and error.
- Selecting a key only changes selection. `NEW`, released push-to-talk, or a
  Review/Debug/Refactor workflow starts simulated work.
- Timed work advances thinking -> running -> waiting/done. Accept and Reject
  only act on a selected waiting agent.
- `HOLD MIC` listens only while the finger remains on the key. Release submits a
  simulated voice task; moving off cancels it.
- Reasoning Low/Medium/High affects the next run duration.
- `SIM` stays visible. There is no BLE, Wi-Fi, USB host control, or mic capture.

## Engineering rules

- `AgentModel` stays free of Arduino/display dependencies and uses fixed storage.
- Controls activate on a valid release in the original target. All primary touch
  targets are at least 40 px high.
- Rendering runs at a bounded 25 fps. Avoid allocation in update/draw paths.
- Check both sprite allocations before binding or drawing BotUx.
- Settings persist with `Preferences`; keep display, audio, simulation speed,
  theme, reduced motion, and Bottom2 LED brightness.
- Keep UI text colors independent of `BotUx::Style::eyeColor`; the shared bot's
  default eyes are intentionally dark.
