# stopwatch — companion watch (M5Stack StopWatch)

A round watch face that hosts the shared `bot-ux` character. There is no elapsed-time
stopwatch mode in this app.

## Hardware

- Device: M5Stack **StopWatch**, ESP32-S3R8 (16 MB flash + 8 MB PSRAM), 1.75" **round
  AMOLED** 466×466 (CO5300 driver, QSPI). PlatformIO `board = esp32s3box` +
  `board_build.arduino.memory_type = qio_opi` + 16 MB partitions.
- Inputs: capacitive **touch** (CST820B) + 2 programmable buttons (BtnA/BtnB). Touch is
  the primary input; buttons are shortcuts.
- Power: **M5PM1** through M5Unified. Battery is derived from filtered VBAT; charging
  requires VIN plus the active-low M5PM1 GPIO2 charge state.
- Audio: ES8311 codec + 1 W speaker via `M5.Speaker.tone()`.
- RTC: RX8130CE via `M5.Rtc`.

## Module layout

```
stopwatch/bot-ux-watch/
  platformio.ini
  include/CalendarMath.h   # deterministic date helpers
  include/WatchFace.h      # owns botux::BotUx + RTC/power face
  include/Settings.h       # NVS-backed preferences
  include/Power.h          # M5PM1 readings + brightness
  src/main.cpp             # face/settings/editors + input/frame loop
  src/WatchFace.cpp
  src/Settings.cpp
  src/Power.cpp
  test/test_calendar_math.cpp
```

## Integration with bot-ux

The hero bot renders into a dedicated 206×206 sprite. A fixed 124×124 sprite provides
the live appearance preview. Both bot sprites and the full-screen sprite are allocated
once in `setup()` and checked before use.

```cpp
M5Canvas canvas(&M5.Display);       // full 466×466
M5Canvas botSprite(&M5.Display);    // 206×206 bot
botSprite.createSprite(206, 206);
bot.begin(&botSprite);              // reads size from the sprite
// each frame:
M5.update();
bot.update(millis());
bot.draw();                          // into botSprite
botSprite.pushSprite(&canvas, 130, 64);
```

The watch draws its own RTC clock, date and battery status. Bot overlays are hidden;
there is no Wi-Fi/signal placeholder. UI text uses a dedicated light ink because the
official bot treatment uses dark pill eyes.

## Input semantics

- Face: tap bot / A pokes; tap time / B toggles seconds; tap SET, swipe up or long A
  opens settings; swipe down or long B enters dim doze.
- Settings: touch rows and explicit minus/plus, Back and Done controls are primary.
  A selects/decrements and B moves/increments as shortcuts; long A backs out and long
  B saves an editor.
- Any touch or A/B press wakes from doze and is consumed, so it cannot trigger the
  control underneath.

## Behavior

- Face: real RTC time/date, measured battery percentage, charging bolt, bot hero and
  visible SET affordance. Low battery uses a red percentage and sleepy pose without
  flashing. A charging transition produces a brief happy acknowledgment.
- Settings: six round-safe rows: Time, Date, Format, Appearance, Brightness and Done.
  Appearance includes theme, organic shape and sound, with live bot preview.
- NVS persists 12/24-hour format, seconds visibility, theme, appearance, brightness
  and sound. RTC hardware persists edited time/date.
- Rendering is time based and capped at 30 fps active / 4 fps dozing. Input and power
  polling continue between frames.

## Rules

- Match existing code style; keep comments purposeful. No per-frame heap allocation.
- Keep the state machine in `main.cpp`; modules are plain classes.
- Persist settings with `Preferences` (NVS). Do not over-engineer edge cases.
- `specs/start-up.md` supersedes older stopwatch-specific notes.
