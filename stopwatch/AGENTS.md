# stopwatch — watch app (M5StickC Plus2)

A watch + stopwatch that hosts the shared `bot-ux` bot as its face and readout.

## Hardware

- Device: M5StickC Plus2, 135×240 portrait ST7789, RGB565. PlatformIO `board = m5stick-c`
  (no dedicated Plus2 board) + `-DBOARD_HAS_PSRAM` + 8MB flash overrides.
- Inputs: **BtnA** (front, GPIO37) and **BtnB** (side, GPIO39). No touch.
- **No PMIC** — battery via ADC on GPIO38 (`M5.Power.getBatteryLevel()`); `isCharging()`
  returns `charge_unknown`, so treat anything but `is_charging` as not-charging.
- Buzzer via `M5.Speaker.tone()`.
- RTC (BM8563) has no coin cell — set time on boot and offer manual set.

## Module layout

```
stopwatch/bot-ux-watch/
  platformio.ini
  include/WatchFace.h      # owns botux::BotUx + clock/battery overlay
  include/Stopwatch.h      # stopwatch state machine + laps
  include/Settings.h       # NVS-backed preferences
  include/Power.h          # battery, brightness, idle sleep
  src/main.cpp             # mode state machine + frame loop
  src/WatchFace.cpp
  src/Stopwatch.cpp
  src/Settings.cpp
  src/Power.cpp
```

## Integration with bot-ux

```cpp
M5Canvas canvas(&M5.Display);
canvas.setColorDepth(16);
canvas.createSprite(M5.Display.width(), M5.Display.height());
botux::BotUx bot;
bot.begin(&canvas);              // reads size from the sprite
// each frame:
M5.update();
bot.update(millis());
bot.draw();
canvas.pushSprite(0, 0);
M5.Display.waitDisplay();
```

The watch draws its **own** large clock; it does not use the bot's `setTime()`/
`setLabel()` overlays (too small at 135 px). It does use `setBattery()`,
`setSignal()`, `setMood()`, `poke()`.

## Button semantics

- TAP (<250 ms) = primary; DOUBLE TAP (2 taps <300 ms) = secondary; LONG (≥600 ms) = back/confirm.
- Single tap is held for a 250 ms disambiguation window before firing.

## Behavior (full spec in `tmp/ux-design.md` PART B)

- Face: Tap A = poke; Double A = 12/24 h toggle; Long A = Settings; Tap B = Stopwatch; Long B = doze.
- Stopwatch: A = start/lap/reset, B = stop/resume; MM:SS.cc, last 3 laps.
- Settings: linear list + value editors (Time set, Format, Theme, Style, Brightness, Back).
- Low battery (≤15 %) → bot forced Sleepy + battery icon blinks; charging → ⚡ + soft Happy.

## Rules

- Match existing code style; keep comments purposeful. No per-frame heap allocation.
- Keep the state machine in `main.cpp`; modules are plain classes, no globals beyond one instance each.
- Persist settings with `Preferences` (NVS). Do not over-engineer edge cases.
- See `tmp/architecture.md` §3a/§5 and `tmp/ux-design.md` PART B for the full design.
