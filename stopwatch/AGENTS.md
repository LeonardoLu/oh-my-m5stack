# stopwatch — watch app (M5Stack StopWatch)

A watch + stopwatch that hosts the shared `bot-ux` bot as its face and readout.

## Hardware

- Device: M5Stack **StopWatch**, ESP32-S3R8 (16 MB flash + 8 MB PSRAM), 1.75" **round
  AMOLED** 466×466 (CO5300 driver, QSPI). PlatformIO `board = esp32s3box` +
  `board_build.arduino.memory_type = qio_opi` + 16 MB partitions.
- Inputs: capacitive **touch** (CST820B) + 2 programmable buttons (BtnA/BtnB). Touch is
  the primary input; buttons are shortcuts.
- Power: **M5PM1** PMU (`M5.Power.getBatteryLevel()` / `isCharging()`).
- Audio: ES8311 codec + 1 W speaker via `M5.Speaker.tone()`.
- RTC: RX8130CE via `M5.Rtc`.

## Module layout

```
stopwatch/bot-ux-watch/
  platformio.ini
  include/WatchFace.h      # owns botux::BotUx + clock/battery overlay
  include/Stopwatch.h      # stopwatch state machine + laps
  include/Settings.h       # NVS-backed preferences
  include/Power.h          # battery, brightness
  src/main.cpp             # mode state machine + frame loop
  src/WatchFace.cpp
  src/Stopwatch.cpp
  src/Settings.cpp
  src/Power.cpp
```

## Integration with bot-ux

The bot renders into a dedicated 200×200 sprite (`botSprite`) that main.cpp pushes onto
the full 466×466 canvas at the top; the watch draws its own large hero clock below it.

```cpp
M5Canvas canvas(&M5.Display);       // full 466×466
M5Canvas botSprite(&M5.Display);    // 200×200 bot
botSprite.createSprite(200, 200);
bot.begin(&botSprite);              // reads size from the sprite
// each frame:
M5.update();
bot.update(millis());
bot.draw();                          // into botSprite
botSprite.pushSprite(&canvas, 133, 15);
```

The watch draws its **own** large clock; it does not use the bot's `setTime()`/
`setLabel()` overlays. It does use `setBattery()`, `setSignal()`, `setMood()`, `poke()`.

## Input semantics

- Touch: tap / swipe-up / swipe-down / long-press (`pollTouch` in main.cpp). Face: tap
  bot = poke, tap clock = stopwatch, swipe up = settings, swipe down = doze.
- Buttons: TAP (<250 ms) = primary; DOUBLE TAP (2 taps <300 ms) = secondary;
  LONG (≥600 ms) = back/confirm.

## Behavior (full spec in `tmp/ux-design.md` PART B)

- Face: tap bot / A = poke; Double A = 12/24 h toggle; swipe up / Long A = Settings;
  tap clock / B = Stopwatch; swipe down / Long B = doze.
- Stopwatch: tap / A = start/lap/reset, B = stop/resume; MM:SS.cc, last 3 laps.
- Settings: tap rows (linear list + value editors: Time set, Format, Theme, Style,
  Brightness, Back); swipe down = save/back.
- Low battery (≤15 %) → bot forced Sleepy + battery icon blinks; charging → ⚡ + soft Happy.

## Rules

- Match existing code style; keep comments purposeful. No per-frame heap allocation.
- Keep the state machine in `main.cpp`; modules are plain classes, no globals beyond one instance each.
- Persist settings with `Preferences` (NVS). Do not over-engineer edge cases.
- See `tmp/architecture.md` §3a/§5 and `tmp/ux-design.md` PART B for the full design.
