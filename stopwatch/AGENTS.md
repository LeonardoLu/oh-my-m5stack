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
  include/WatchInteraction.h # chord, scroll, motion filter and ambient timing
  include/Power.h          # M5PM1 readings + brightness
  src/main.cpp             # face/settings/editors + input/frame loop
  src/WatchFace.cpp
  src/Settings.cpp
  src/Power.cpp
  test/test_calendar_math.cpp
```

## Integration with bot-ux

The hero bot renders into a dedicated 310×310 sprite. A fixed 178×178 sprite provides
the live settings preview. Both bot sprites and the full-screen sprite are allocated
once in `setup()` and checked before use.

```cpp
M5Canvas canvas(&M5.Display);       // full 466×466
M5Canvas botSprite(&M5.Display);    // 310×310 bot
botSprite.createSprite(310, 310);
bot.begin(&botSprite);              // reads size from the sprite
// each frame:
M5.update();
bot.update(millis());
bot.draw();                          // into botSprite
botSprite.pushSprite(&M5.Display, 78, 66);
```

The watch draws its own RTC clock, date and battery status. Bot overlays are hidden;
there is no Wi-Fi/signal placeholder. UI text uses a dedicated light ink because the
official bot treatment uses dark pill eyes.

## Input semantics

- Face: tap bot / A pokes; long-pressing the bot opens Bot Personality; horizontal
  swipe / B cycles expressions; tap the combined time/date block toggles seconds.
  Settings has no visible face affordance and opens only after A+B remain held together
  for 3 seconds. Long B enters dim doze.
- A downward drag beginning in the top 20 px and travelling more than 50 px reveals
  the compact battery panel. Tap the panel to dismiss it; otherwise it closes after
  six seconds.
- Settings and Bot Personality use four large touch rows at a time. Vertical swipes
  scroll/select; every editor has touch controls plus Back and Done. Buttons remain
  optional shortcuts.
- Any touch or A/B press wakes from doze and is consumed, so it cannot trigger the
  control underneath.

## Behavior

- Face: RTC year/month/day/weekday and time share one lower information block. Battery
  has no permanent percentage; its factory-inspired top panel slides in over 300 ms
  and shows percentage, a horizontal gauge, green charging fill and bolt.
- Settings is a scrollable hierarchy: clock, Bot Personality, display/sound and Done.
  Personality includes expression, action, shape, eye style, HSV color, action amount
  and speed. The HSV picker previews live and Done persists it.
- NVS persists 12/24-hour format, seconds, theme, shape, eye style, custom HSV body
  color, expression, animation, wrist response, motion amount/speed, brightness and
  sound. RTC hardware persists time/date.
- BMI270 tilt passes through a low-pass filter and dead zone. Shake uses hysteresis and
  a seven-second cooldown before poke. Auto expression shuffles among calm ambient moods
  every 26–48 seconds; direct interaction postpones rotation for 60 seconds.
- Rendering targets 16 ms active / 33 ms preview / 250 ms dozing. The face pushes only
  the bot region; disjoint clock/status regions redraw when their values change. Serial
  `PERF` summaries report real frame timing for hardware validation.
- Serial `c` remains raw RGB565 capture. Diagnostic pages accept `vNN` plus newline:
  0 face, 1 settings, 2 personality, 3 expression, 4 appearance, 5 motion, 6 color,
  7 display, 8 scrolled settings, 9 scrolled personality, 10 battery panel, 11 format.
  Single digits remain accepted for compatibility; page selection never saves NVS.

## Rules

- Match existing code style; keep comments purposeful. No per-frame heap allocation.
- Keep the state machine in `main.cpp`; modules are plain classes.
- Persist settings with `Preferences` (NVS). Do not over-engineer edge cases.
- The user's current goal and `specs/ux-polish.md` supersede older stopwatch notes.
