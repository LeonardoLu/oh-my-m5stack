// bot-ux-watch — a round-safe M5Stack StopWatch companion face.

#include <M5Unified.h>
#include <BotUx.h>

#include "CalendarMath.h"
#include "InputSemantics.h"
#include "Power.h"
#include "Settings.h"
#include "TimedState.h"
#include "WatchFace.h"

#include <esp_heap_caps.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum class Screen : uint8_t { Face, Settings, Editor };
enum class Editor : uint8_t { None, Time, Date, Format, Expression, Appearance, Motion, Brightness };
enum class MenuItem : uint8_t { Time, Date, Format, Expression, Appearance, Motion, Brightness, Done, Count };
using watchinput::Gesture;

namespace {
constexpr int16_t kW = 466;
constexpr int16_t kH = 466;
constexpr int16_t kBotSize = 310;
constexpr int16_t kBotX = (kW - kBotSize) / 2;
constexpr int16_t kBotY = 66;
constexpr int16_t kPreviewSize = 178;
constexpr int16_t kPreviewX = (kW - kPreviewSize) / 2;
constexpr int16_t kPreviewY = 62;
constexpr int16_t kMenuY = 82;
constexpr int16_t kMenuStep = 43;
constexpr uint32_t kActiveFrameMs = 16;
constexpr uint32_t kPreviewFrameMs = 33;
constexpr uint32_t kDozeFrameMs = 250;
constexpr uint8_t kLowBattery = 15;

const char* const kMenuLabels[(uint8_t)MenuItem::Count] = {
    "TIME", "DATE", "FORMAT", "EXPRESSION", "APPEARANCE", "MOTION", "BRIGHTNESS", "DONE"
};
const char* const kMonths[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
}

M5Canvas canvas(&M5.Display);
M5Canvas botSprite(&M5.Display);
M5Canvas previewSprite(&M5.Display);
WatchFace face;
botux::BotUx previewBot;
Settings settings;
Power power;

Screen _screen = Screen::Face;
Editor _editor = Editor::None;
MenuItem _menu = MenuItem::Time;
Settings::Data _originalSettings;

watchinput::ButtonGesture _buttonA;
watchinput::ButtonGesture _buttonB;
watchinput::TouchGesture _touch;
int16_t _tapX = 0, _tapY = 0;

uint8_t _battery = 100;
bool _charging = false;
bool _powerKnown = false;
uint32_t _powerReadMs = 0;
TimedState _happyAcknowledgment;
TimedState _statusPanel;
bool _dozing = false;
bool _renderReady = false;
bool _uiDirty = true;
bool _faceNeedsClear = true;
uint32_t _lastFrameMs = 0;
uint32_t _lastImuReadMs = 0;
uint32_t _lastShakeMs = 0;

uint8_t _editHour = 0;
uint8_t _editMinute = 0;
int16_t _editYear = 2026;
uint8_t _editMonth = 1;
uint8_t _editDay = 1;
uint8_t _editField = 0;

struct FrameTelemetry {
    uint32_t windowStartMs = 0;
    uint32_t frames = 0;
    uint64_t updateUs = 0;
    uint64_t drawUs = 0;
    uint64_t pushUs = 0;
    uint32_t maxFrameUs = 0;
    uint8_t screen = 0xFF;
} _telemetry;

static bool hit(int16_t x, int16_t y, int16_t left, int16_t top, int16_t width, int16_t height) {
    return x >= left && x < left + width && y >= top && y < top + height;
}

static void playTone(uint16_t frequency, uint16_t duration) {
    if (settings.data().sound) M5.Speaker.tone(frequency, duration);
}

static void clickSound()   { playTone(960, 24); }
static void confirmSound() { playTone(1260, 55); }
static void pokeSound()    { playTone(1420, 36); }

static void consumeWakeInput() {
    _buttonA.consume(M5.BtnA.isPressed());
    _buttonB.consume(M5.BtnB.isPressed());
    _touch.consume();
}

static void applySettings() {
    settings.rebuildStyle();
    settings.apply(face.bot());
    settings.apply(previewBot);
    auto expression = (botux::BotUx::Expression)settings.data().expression;
    auto animation = (botux::BotUx::Animation)settings.data().animation;
    face.bot().setExpression(expression);
    previewBot.setExpression(expression);
    face.bot().setAnimation(animation);
    previewBot.setAnimation(animation);
    face.bot().setReducedMotion(!settings.data().motion);
    previewBot.setReducedMotion(!settings.data().motion);
    face.bot().setMotionAmount(1.0f);
    previewBot.setMotionAmount(1.0f);
    face.setHour24(settings.data().hour24);
    face.setShowSeconds(settings.data().showSeconds);
    power.applyLevel(settings.data().brightness);
    _uiDirty = true;
}

static void seedRtcIfNeeded() {
    auto dt = M5.Rtc.getDateTime();
    bool valid = dt.date.year >= 2020 && dt.date.year <= 2099
        && dt.date.month >= 1 && dt.date.month <= 12
        && dt.date.date >= 1 && dt.date.date <= 31
        && dt.time.hours >= 0 && dt.time.hours <= 23
        && dt.time.minutes >= 0 && dt.time.minutes <= 59;
    if (valid) return;

    int year = 2026, month = 1, day = 1, hour = 0, minute = 0, second = 0;
    char mon[4] = {};
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* found = strstr(months, mon);
    if (found) month = (int)((found - months) / 3 + 1);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

    dt.date.year = (int16_t)year;
    dt.date.month = (int8_t)month;
    dt.date.date = (int8_t)day;
    dt.date.weekDay = (int8_t)watchcalendar::weekDay(year, month, day);
    dt.time.hours = (int8_t)hour;
    dt.time.minutes = (int8_t)minute;
    dt.time.seconds = (int8_t)second;
    M5.Rtc.setDateTime(dt);
}

static void refreshPower(uint32_t now) {
    if (_powerKnown && now - _powerReadMs < 1000) return;
    _powerReadMs = now;
    bool wasCharging = _charging;
    power.update();
    _battery = power.batteryPct();
    _charging = power.charging();
    if (_powerKnown && _charging && !wasCharging) {
        _happyAcknowledgment.start(now, 1400);
        _statusPanel.start(now, 3200);
        confirmSound();
    }
    _powerKnown = true;
}

static botux::BotUx::Mood faceMood(uint32_t now) {
    if (_dozing) return botux::BotUx::Mood::Sleepy;
    if (_happyAcknowledgment.active(now)) return botux::BotUx::Mood::Happy;
    if (_battery <= kLowBattery) return botux::BotUx::Mood::Sleepy;
    if (_screen != Screen::Face) return botux::BotUx::Mood::Listening;
    return botux::BotUx::Mood::Idle;
}

static void enterSettings() {
    _screen = Screen::Settings;
    _editor = Editor::None;
    _menu = MenuItem::Time;
    _uiDirty = true;
    confirmSound();
}

static void leaveSettings() {
    _screen = Screen::Face;
    _editor = Editor::None;
    settings.save();
    _faceNeedsClear = true;
    face.invalidate();
    confirmSound();
}

static void enterEditor(Editor editor) {
    _screen = Screen::Editor;
    _editor = editor;
    _originalSettings = settings.data();
    _editField = 0;
    _uiDirty = true;
    auto dt = M5.Rtc.getDateTime();
    _editHour = (uint8_t)dt.time.hours;
    _editMinute = (uint8_t)dt.time.minutes;
    _editYear = dt.date.year;
    _editMonth = (uint8_t)dt.date.month;
    _editDay = (uint8_t)dt.date.date;
    clickSound();
}

static void cancelEditor() {
    settings.data() = _originalSettings;
    applySettings();
    _screen = Screen::Settings;
    _editor = Editor::None;
    _uiDirty = true;
    clickSound();
}

static void saveEditor() {
    if (_editor == Editor::Time) {
        auto dt = M5.Rtc.getDateTime();
        dt.time.hours = (int8_t)_editHour;
        dt.time.minutes = (int8_t)_editMinute;
        dt.time.seconds = 0;
        M5.Rtc.setDateTime(dt);
    } else if (_editor == Editor::Date) {
        auto dt = M5.Rtc.getDateTime();
        dt.date.year = _editYear;
        dt.date.month = (int8_t)_editMonth;
        dt.date.date = (int8_t)_editDay;
        dt.date.weekDay = (int8_t)watchcalendar::weekDay(_editYear, _editMonth, _editDay);
        M5.Rtc.setDateTime(dt);
    } else {
        settings.save();
    }
    _screen = Screen::Settings;
    _editor = Editor::None;
    _uiDirty = true;
    confirmSound();
}

static void toggleSeconds() {
    settings.data().showSeconds = !settings.data().showSeconds;
    face.setShowSeconds(settings.data().showSeconds);
    settings.save();
    _uiDirty = true;
    clickSound();
}

static void pokeBot() {
    face.bot().poke();
    pokeSound();
}

static void startDoze() {
    _dozing = true;
    power.setBrightness(14);
}

static void cycleExpression(int8_t delta, bool persist = false) {
    settings.data().expression = (uint8_t)((settings.data().expression
        + Settings::EXPRESSION_COUNT + delta) % Settings::EXPRESSION_COUNT);
    auto expression = (botux::BotUx::Expression)settings.data().expression;
    face.bot().setExpression(expression);
    previewBot.setExpression(expression);
    _uiDirty = true;
    if (persist) settings.save();
    clickSound();
}

static float clampMotion(float value) {
    if (value < -1.0f) return -1.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static void updateMotion(uint32_t now) {
    if (now - _lastImuReadMs < 20) return;
    _lastImuReadMs = now;

    if (_dozing) {
        face.bot().setMotion(0.0f, 0.0f, 0.0f);
        return;
    }

    float ax = 0.0f, ay = 0.0f, az = 1.0f;
    float gx = 0.0f, gy = 0.0f, gz = 0.0f;
    bool accelOk = false, gyroOk = false;
    if (settings.data().motion && M5.Imu.isEnabled()) {
        M5.Imu.update();
        accelOk = M5.Imu.getAccel(&ax, &ay, &az);
        gyroOk = M5.Imu.getGyro(&gx, &gy, &gz);
    }

    float shake = 0.0f;
    if (gyroOk) {
        shake = sqrtf(gx * gx + gy * gy + gz * gz) / 180.0f;
        if (shake > 1.0f) shake = 1.0f;
    }
    float tiltX = accelOk ? clampMotion(ax) : 0.0f;
    float tiltY = accelOk ? clampMotion(ay) : 0.0f;
    face.bot().setMotion(tiltX, tiltY, shake);
    previewBot.setMotion(tiltX, tiltY, shake);

    if (shake > 0.82f && now - _lastShakeMs > 1400) {
        _lastShakeMs = now;
        face.bot().poke();
        pokeSound();
    }
}

static void changeEditorValue(int8_t delta) {
    if (_editor == Editor::Time) {
        if (_editField == 0) _editHour = (uint8_t)((_editHour + 24 + delta) % 24);
        else _editMinute = (uint8_t)((_editMinute + 60 + delta) % 60);
    } else if (_editor == Editor::Date) {
        if (_editField == 0) {
            _editMonth = (uint8_t)((_editMonth - 1 + 12 + delta) % 12 + 1);
        } else if (_editField == 1) {
            int maxDay = watchcalendar::daysInMonth(_editYear, _editMonth);
            _editDay = (uint8_t)((_editDay - 1 + maxDay + delta) % maxDay + 1);
        } else {
            _editYear = (int16_t)((_editYear - 2020 + 80 + delta) % 80 + 2020);
        }
        uint8_t maxDay = watchcalendar::daysInMonth(_editYear, _editMonth);
        if (_editDay > maxDay) _editDay = maxDay;
    } else if (_editor == Editor::Format) {
        if (_editField == 0) settings.data().hour24 = !settings.data().hour24;
        else settings.data().showSeconds = !settings.data().showSeconds;
        applySettings();
    } else if (_editor == Editor::Expression) {
        cycleExpression(delta);
    } else if (_editor == Editor::Appearance) {
        if (_editField == 0) {
            settings.data().theme = (uint8_t)((settings.data().theme + Settings::THEME_COUNT + delta) % Settings::THEME_COUNT);
        } else if (_editField == 1) {
            settings.data().appearance = (uint8_t)((settings.data().appearance + Settings::APPEARANCE_COUNT + delta) % Settings::APPEARANCE_COUNT);
        } else {
            bool enabling = !settings.data().sound;
            if (!enabling) clickSound();
            settings.data().sound = enabling;
        }
        applySettings();
    } else if (_editor == Editor::Motion) {
        if (_editField == 0) {
            settings.data().animation = (uint8_t)((settings.data().animation
                + Settings::ANIMATION_COUNT + delta) % Settings::ANIMATION_COUNT);
        } else {
            settings.data().motion = !settings.data().motion;
        }
        applySettings();
    } else if (_editor == Editor::Brightness) {
        int level = settings.data().brightness + delta;
        if (level < 1) level = 1;
        if (level > 5) level = 5;
        settings.data().brightness = (uint8_t)level;
        applySettings();
    }
    if (!(_editor == Editor::Appearance && _editField == 2 && !settings.data().sound)) clickSound();
}

static void selectMenuItem() {
    switch (_menu) {
        case MenuItem::Time:       enterEditor(Editor::Time); break;
        case MenuItem::Date:       enterEditor(Editor::Date); break;
        case MenuItem::Format:     enterEditor(Editor::Format); break;
        case MenuItem::Expression: enterEditor(Editor::Expression); break;
        case MenuItem::Appearance: enterEditor(Editor::Appearance); break;
        case MenuItem::Motion:     enterEditor(Editor::Motion); break;
        case MenuItem::Brightness: enterEditor(Editor::Brightness); break;
        case MenuItem::Done:       leaveSettings(); break;
        default: break;
    }
}

static void handleFaceInput(Gesture gesture) {
    if (gesture == Gesture::Tap) {
        if (hit(_tapX, _tapY, 154, 420, 158, 46)) enterSettings();
        else if (hit(_tapX, _tapY, 112, 354, 242, 72)) toggleSeconds();
        else if (hit(_tapX, _tapY, kBotX, kBotY, kBotSize, kBotSize)) pokeBot();
    } else if (gesture == Gesture::SwipeUp) {
        enterSettings();
    } else if (gesture == Gesture::SwipeDown) {
        if (_touch.startY() <= 28) {
            _statusPanel.start(millis(), 3200);
            _uiDirty = true;
        } else {
            startDoze();
        }
    } else if (gesture == Gesture::SwipeLeft) {
        cycleExpression(1, true);
    } else if (gesture == Gesture::SwipeRight) {
        cycleExpression(-1, true);
    }
}

static void handleSettingsTouch() {
    for (uint8_t i = 0; i < (uint8_t)MenuItem::Count; ++i) {
        int16_t cy = kMenuY + i * kMenuStep;
        if (hit(_tapX, _tapY, 58, cy - 23, 350, 46)) {
            _menu = (MenuItem)i;
            selectMenuItem();
            return;
        }
    }
}

static void handleEditorTouch() {
    if (hit(_tapX, _tapY, 84, 388, 138, 54)) { cancelEditor(); return; }
    if (hit(_tapX, _tapY, 244, 388, 138, 54)) { saveEditor(); return; }

    if (_editor == Editor::Time) {
        const int16_t centers[2] = { 157, 309 };
        for (uint8_t i = 0; i < 2; ++i) {
            if (hit(_tapX, _tapY, centers[i] - 50, 115, 100, 54)) { _editField = i; changeEditorValue(-1); return; }
            if (hit(_tapX, _tapY, centers[i] - 50, 255, 100, 54)) { _editField = i; changeEditorValue(1); return; }
            if (hit(_tapX, _tapY, centers[i] - 55, 175, 110, 72)) { _editField = i; clickSound(); return; }
        }
    } else if (_editor == Editor::Date) {
        const int16_t centers[3] = { 108, 233, 358 };
        for (uint8_t i = 0; i < 3; ++i) {
            if (hit(_tapX, _tapY, centers[i] - 43, 115, 86, 52)) { _editField = i; changeEditorValue(-1); return; }
            if (hit(_tapX, _tapY, centers[i] - 43, 255, 86, 52)) { _editField = i; changeEditorValue(1); return; }
            if (hit(_tapX, _tapY, centers[i] - 46, 175, 92, 72)) { _editField = i; clickSound(); return; }
        }
    } else if (_editor == Editor::Format) {
        if (hit(_tapX, _tapY, 78, 128, 146, 76)) {
            _editField = 0;
            settings.data().hour24 = false; applySettings(); clickSound();
        } else if (hit(_tapX, _tapY, 242, 128, 146, 76)) {
            _editField = 0;
            settings.data().hour24 = true; applySettings(); clickSound();
        } else if (hit(_tapX, _tapY, 100, 242, 266, 58)) {
            _editField = 1;
            settings.data().showSeconds = !settings.data().showSeconds;
            applySettings(); clickSound();
        }
    } else if (_editor == Editor::Expression) {
        if (hit(_tapX, _tapY, 48, 126, 76, 96)) cycleExpression(-1);
        else if (hit(_tapX, _tapY, 342, 126, 76, 96)) cycleExpression(1);
        else if (hit(_tapX, _tapY, kPreviewX, kPreviewY, kPreviewSize, kPreviewSize)) {
            previewBot.poke(); pokeSound();
        }
    } else if (_editor == Editor::Appearance) {
        const int16_t rows[3] = { 264, 312, 360 };
        for (uint8_t i = 0; i < 3; ++i) {
            if (hit(_tapX, _tapY, 58, rows[i] - 23, 350, 46)) {
                _editField = i;
                int8_t delta = (_tapX < 233) ? -1 : 1;
                changeEditorValue(delta);
                return;
            }
        }
    } else if (_editor == Editor::Motion) {
        const int16_t rows[2] = { 274, 334 };
        for (uint8_t i = 0; i < 2; ++i) {
            if (hit(_tapX, _tapY, 58, rows[i] - 24, 350, 48)) {
                _editField = i;
                int8_t delta = (_tapX < 233) ? -1 : 1;
                changeEditorValue(delta);
                return;
            }
        }
    } else if (_editor == Editor::Brightness) {
        if (hit(_tapX, _tapY, 72, 160, 96, 76)) changeEditorValue(-1);
        else if (hit(_tapX, _tapY, 298, 160, 96, 76)) changeEditorValue(1);
    }
}

static void handleInputs(uint32_t now) {
    auto touch = M5.Touch.getDetail(0);
    bool wakePressed = M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || touch.wasPressed();
    if (_dozing) {
        if (wakePressed) {
            _dozing = false;
            consumeWakeInput();
            power.applyLevel(settings.data().brightness);
        }
        return;
    }

    Gesture ga = _buttonA.poll(M5.BtnA.wasPressed(), M5.BtnA.isPressed(), M5.BtnA.wasReleased(), now);
    Gesture gb = _buttonB.poll(M5.BtnB.wasPressed(), M5.BtnB.isPressed(), M5.BtnB.wasReleased(), now);
    Gesture gt = _touch.poll(touch.wasPressed(), touch.isPressed(), touch.wasReleased(),
                             (int16_t)touch.x, (int16_t)touch.y, now);
    if (gt == Gesture::Tap) {
        _tapX = _touch.tapX();
        _tapY = _touch.tapY();
    }

    if (_screen == Screen::Face) {
        if (ga == Gesture::Tap) pokeBot();
        else if (ga == Gesture::Long) enterSettings();
        if (gb == Gesture::Tap) cycleExpression(1, true);
        else if (gb == Gesture::Long) startDoze();
        if (gt != Gesture::None) handleFaceInput(gt);
        return;
    }

    if (_screen == Screen::Settings) {
        if (ga == Gesture::Tap) selectMenuItem();
        else if (ga == Gesture::Long) leaveSettings();
        if (gb == Gesture::Tap) {
            _menu = (MenuItem)(((uint8_t)_menu + 1) % (uint8_t)MenuItem::Count);
            _uiDirty = true;
            clickSound();
        }
        if (gt == Gesture::Tap) handleSettingsTouch();
        else if (gt == Gesture::SwipeDown) leaveSettings();
        return;
    }

    if (ga == Gesture::Tap) changeEditorValue(-1);
    else if (ga == Gesture::Long) cancelEditor();
    if (gb == Gesture::Tap) changeEditorValue(1);
    else if (gb == Gesture::Long) saveEditor();
    if (gt == Gesture::Tap) handleEditorTouch();
    else if (gt == Gesture::SwipeDown) cancelEditor();
    else if (_editor == Editor::Expression && gt == Gesture::SwipeLeft) cycleExpression(1);
    else if (_editor == Editor::Expression && gt == Gesture::SwipeRight) cycleExpression(-1);
}

static void drawPill(int16_t x, int16_t y, int16_t w, int16_t h,
                     const char* label, bool selected, bool accentFill = false) {
    uint16_t fill = accentFill ? settings.style().accentColor : settings.panel();
    int16_t radius = ((w < h) ? w : h) / 2;
    canvas.fillRoundRect(x, y, w, h, radius, fill);
    canvas.drawRoundRect(x, y, w, h, radius,
                         selected ? settings.style().accentColor : settings.muted());
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(accentFill ? settings.style().bgColor : settings.ink());
    canvas.drawString(label, x + w / 2, y + h / 2);
}

static void drawTitle(const char* title) {
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.ink());
    canvas.drawString(title, kW / 2, 48);
}

static void drawSettingsList() {
    drawTitle("SETTINGS");
    for (uint8_t i = 0; i < (uint8_t)MenuItem::Count; ++i) {
        int16_t cy = kMenuY + i * kMenuStep;
        bool selected = i == (uint8_t)_menu;
        if (selected) canvas.fillRoundRect(78, cy - 19, 310, 38, 19, settings.panel());

        canvas.setTextDatum(middle_left);
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextSize(1.0f);
        canvas.setTextColor(selected ? settings.style().accentColor : settings.ink());
        canvas.drawString(kMenuLabels[i], 88, cy);

        char value[24] = {};
        switch ((MenuItem)i) {
            case MenuItem::Time: snprintf(value, sizeof(value), "%02u:%02u", face.hour(), face.minute()); break;
            case MenuItem::Date: {
                uint8_t month = face.month();
                const char* name = month >= 1 && month <= 12 ? kMonths[month - 1] : "---";
                snprintf(value, sizeof(value), "%02u %s", face.day(), name);
                break;
            }
            case MenuItem::Format: snprintf(value, sizeof(value), settings.data().hour24 ? "24 H" : "12 H"); break;
            case MenuItem::Expression: snprintf(value, sizeof(value), "%s", Settings::expressionName(settings.data().expression)); break;
            case MenuItem::Appearance: snprintf(value, sizeof(value), "%s", Settings::appearanceName(settings.data().appearance)); break;
            case MenuItem::Motion: snprintf(value, sizeof(value), "%s", settings.data().motion ? "FULL" : "REDUCED"); break;
            case MenuItem::Brightness: snprintf(value, sizeof(value), "%u / 5", settings.data().brightness); break;
            default: break;
        }
        if (value[0]) {
            canvas.setTextDatum(middle_right);
            canvas.setTextColor(settings.muted());
            canvas.drawString(value, 378, cy);
        }
    }
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted());
    canvas.drawString("A SELECT     B MOVE", 233, 425);
}

static void drawFooter() {
    drawPill(84, 390, 138, 48, "BACK", false);
    drawPill(244, 390, 138, 48, "DONE", true, true);
}

static void drawStepper(int16_t cx, const char* label, const char* value, bool selected, int16_t width) {
    drawPill(cx - width / 2, 116, width, 52, "-", selected);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted());
    canvas.drawString(label, cx, 190);
    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(selected ? settings.style().accentColor : settings.ink());
    canvas.drawString(value, cx, 220);
    drawPill(cx - width / 2, 256, width, 52, "+", selected);
}

static void drawTimeEditor() {
    drawTitle("SET TIME");
    char hour[4], minute[4];
    snprintf(hour, sizeof(hour), "%02u", (unsigned)_editHour);
    snprintf(minute, sizeof(minute), "%02u", (unsigned)_editMinute);
    drawStepper(157, "HOUR", hour, _editField == 0, 100);
    drawStepper(309, "MINUTE", minute, _editField == 1, 100);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(settings.muted());
    canvas.drawString(":", 233, 220);
    drawFooter();
}

static void drawDateEditor() {
    drawTitle("SET DATE");
    char day[4], year[6];
    snprintf(day, sizeof(day), "%02u", (unsigned)_editDay);
    snprintf(year, sizeof(year), "%d", (int)_editYear);
    drawStepper(108, "MONTH", kMonths[_editMonth - 1], _editField == 0, 86);
    drawStepper(233, "DAY", day, _editField == 1, 86);
    drawStepper(358, "YEAR", year, _editField == 2, 86);
    drawFooter();
}

static void drawFormatEditor() {
    drawTitle("TIME FORMAT");
    drawPill(78, 128, 146, 76, "12 HOUR", !settings.data().hour24, !settings.data().hour24);
    drawPill(242, 128, 146, 76, "24 HOUR", settings.data().hour24, settings.data().hour24);
    drawPill(100, 242, 266, 58, settings.data().showSeconds ? "SECONDS  ON" : "SECONDS  OFF",
             _editField == 1, settings.data().showSeconds);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted());
    canvas.drawString("Tap the time on the watch face to toggle", 233, 330);
    drawFooter();
}

static void drawArrowRow(int16_t cy, const char* label, const char* value, bool selected) {
    if (selected) canvas.fillRoundRect(58, cy - 22, 350, 44, 17, settings.panel());
    canvas.setTextDatum(middle_left);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted());
    canvas.drawString(label, 76, cy);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(settings.ink());
    canvas.drawString(value, 270, cy);
    canvas.setTextColor(settings.style().accentColor);
    canvas.drawString("<", 205, cy);
    canvas.drawString(">", 388, cy);
}

static void drawAppearanceEditor() {
    drawTitle("APPEARANCE");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, kPreviewY);
    drawArrowRow(264, "THEME", Settings::themeName(settings.data().theme), _editField == 0);
    drawArrowRow(312, "SHAPE", Settings::appearanceName(settings.data().appearance), _editField == 1);
    drawArrowRow(360, "SOUND", settings.data().sound ? "ON" : "OFF", _editField == 2);
    drawFooter();
}

static void drawExpressionEditor() {
    drawTitle("EXPRESSION");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, kPreviewY);
    drawPill(48, 126, 76, 96, "<", false);
    drawPill(342, 126, 76, 96, ">", false);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(settings.ink());
    canvas.drawString(Settings::expressionName(settings.data().expression), 233, 274);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(settings.muted());
    canvas.drawString("Swipe or tap arrows; tap the bot to react", 233, 326);
    drawFooter();
}

static void drawMotionEditor() {
    drawTitle("MOTION");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, kPreviewY);
    drawArrowRow(274, "ANIMATION", Settings::animationName(settings.data().animation), _editField == 0);
    drawArrowRow(334, "WRIST", settings.data().motion ? "FULL" : "REDUCED", _editField == 1);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(settings.muted());
    canvas.drawString(settings.data().motion ? "Tilt and shake to move the bot" : "Gentle animation, IMU response off", 233, 374);
    drawFooter();
}

static void drawBrightnessEditor() {
    drawTitle("BRIGHTNESS");
    drawPill(72, 160, 96, 76, "-", false);
    drawPill(298, 160, 96, 76, "+", false);
    for (uint8_t i = 0; i < 5; ++i) {
        uint16_t color = i < settings.data().brightness
            ? settings.style().accentColor : settings.panel();
        canvas.fillRoundRect(185 + i * 22, 187, 16, 22, 6, color);
    }
    char value[8];
    snprintf(value, sizeof(value), "%u / 5", settings.data().brightness);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.ink());
    canvas.drawString(value, 233, 278);
    drawFooter();
}

static void drawEditor() {
    switch (_editor) {
        case Editor::Time:       drawTimeEditor(); break;
        case Editor::Date:       drawDateEditor(); break;
        case Editor::Format:     drawFormatEditor(); break;
        case Editor::Expression: drawExpressionEditor(); break;
        case Editor::Appearance: drawAppearanceEditor(); break;
        case Editor::Motion:     drawMotionEditor(); break;
        case Editor::Brightness: drawBrightnessEditor(); break;
        default: break;
    }
}

static bool previewIsAnimated() {
    return _screen == Screen::Editor
        && (_editor == Editor::Expression || _editor == Editor::Appearance || _editor == Editor::Motion);
}

static void recordFrame(uint32_t now, uint32_t updateUs, uint32_t drawUs, uint32_t pushUs) {
    uint8_t screen = (uint8_t)_screen;
    if (_telemetry.screen != screen) {
        _telemetry = {};
        _telemetry.screen = screen;
    }
    if (_telemetry.windowStartMs == 0) _telemetry.windowStartMs = now;
    ++_telemetry.frames;
    _telemetry.updateUs += updateUs;
    _telemetry.drawUs += drawUs;
    _telemetry.pushUs += pushUs;
    uint32_t frameUs = updateUs + drawUs + pushUs;
    if (frameUs > _telemetry.maxFrameUs) _telemetry.maxFrameUs = frameUs;
    uint32_t elapsed = now - _telemetry.windowStartMs;
    if (elapsed < 5000 || _telemetry.frames == 0) return;

    Serial.printf("PERF screen=%u fps=%.1f update=%luus draw=%luus push=%luus max=%luus heap=%u largest=%u psram=%u\n",
                  (unsigned)_screen, (double)_telemetry.frames * 1000.0 / elapsed,
                  (unsigned long)(_telemetry.updateUs / _telemetry.frames),
                  (unsigned long)(_telemetry.drawUs / _telemetry.frames),
                  (unsigned long)(_telemetry.pushUs / _telemetry.frames),
                  (unsigned long)_telemetry.maxFrameUs,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)ESP.getFreePsram());
    _telemetry = {};
    _telemetry.windowStartMs = now;
    _telemetry.screen = screen;
}

static void render(uint32_t now) {
    uint32_t t0 = micros();
    face.setBattery(_battery);
    face.setCharging(_charging);
    face.setHour24(settings.data().hour24);
    face.setShowSeconds(settings.data().showSeconds);
    face.bot().setMood(faceMood(now));
    auto selectedExpression = (botux::BotUx::Expression)settings.data().expression;
    face.bot().setExpression((_dozing || _battery <= kLowBattery)
        ? botux::BotUx::Expression::Auto : selectedExpression);
    face.update(now);
    face.bot().update(now);

    if (previewIsAnimated()) {
        previewBot.setMood(botux::BotUx::Mood::Listening);
        previewBot.update(now);
    }
    uint32_t t1 = micros();

    if (_screen == Screen::Face) {
        if (_faceNeedsClear) {
            M5.Display.fillScreen(settings.style().bgColor);
            _faceNeedsClear = false;
        }
        face.bot().draw();
        uint32_t t2 = micros();
        botSprite.pushSprite(&M5.Display, kBotX, kBotY);
        face.draw(&M5.Display, settings.style().bgColor, settings.ink(), settings.muted(),
                  settings.style().accentColor, settings.panel(), settings.warning(),
                  _statusPanel.active(now));
        M5.Display.waitDisplay();
        uint32_t t3 = micros();
        recordFrame(now, t1 - t0, t2 - t1, t3 - t2);
        return;
    }

    if (!_uiDirty && !previewIsAnimated()) return;
    canvas.fillSprite(settings.style().bgColor);
    if (_screen == Screen::Settings) {
        drawSettingsList();
    } else {
        drawEditor();
    }
    uint32_t t2 = micros();
    canvas.pushSprite(0, 0);
    M5.Display.waitDisplay();
    uint32_t t3 = micros();
    _uiDirty = false;
    recordFrame(now, t1 - t0, t2 - t1, t3 - t2);
}

static void emitFrameCapture() {
    if (_screen == Screen::Face) {
        canvas.fillSprite(settings.style().bgColor);
        botSprite.pushSprite(&canvas, kBotX, kBotY);
        face.invalidate();
        face.draw(&canvas, settings.style().bgColor, settings.ink(), settings.muted(),
                  settings.style().accentColor, settings.panel(), settings.warning(),
                  _statusPanel.active(millis()));
    }

    const uint8_t* pixels = (const uint8_t*)canvas.getBuffer();
    size_t bytes = canvas.bufferLength();
    Serial.printf("FRAME %d %d RGB565BE %u\n", kW, kH, (unsigned)bytes);
    for (size_t offset = 0; offset < bytes; offset += 2048) {
        size_t chunk = bytes - offset;
        if (chunk > 2048) chunk = 2048;
        Serial.write(pixels + offset, chunk);
        yield();
    }
    Serial.print("\nFRAME_END\n");
    Serial.flush();
}

static void handleSerialCommands() {
    while (Serial.available()) {
        char command = (char)Serial.read();
        if (command == 'e') cycleExpression(1, true);
        else if (command == 'a') {
            settings.data().animation = (settings.data().animation + 1) % Settings::ANIMATION_COUNT;
            applySettings();
            settings.save();
        } else if (command == 'm') {
            settings.data().motion = !settings.data().motion;
            applySettings();
            settings.save();
        } else if (command == 'p') pokeBot();
        else if (command == 's') _statusPanel.start(millis(), 3200);
        else if (command == 'c') emitFrameCapture();
        else if (command >= '0' && command <= '4') {
            _originalSettings = settings.data();
            if (command == '0') {
                _screen = Screen::Face;
                _editor = Editor::None;
                _faceNeedsClear = true;
                face.invalidate();
            } else if (command == '1') {
                _screen = Screen::Settings;
                _editor = Editor::None;
            } else {
                _screen = Screen::Editor;
                _editor = command == '2' ? Editor::Expression
                        : command == '3' ? Editor::Appearance : Editor::Motion;
                _editField = 0;
            }
            _uiDirty = true;
        }
    }
}

void setup() {
    auto cfg = M5.config();
    cfg.internal_imu = true;
    M5.begin(cfg);
    Serial.begin(115200);

    settings.begin();
    power.begin();
    seedRtcIfNeeded();

    canvas.setColorDepth(16);
    botSprite.setColorDepth(16);
    previewSprite.setColorDepth(16);
    canvas.setPsram(true);
    botSprite.setPsram(true);
    previewSprite.setPsram(false);
    bool canvasOk = canvas.createSprite(kW, kH) != nullptr;
    bool botOk = botSprite.createSprite(kBotSize, kBotSize) != nullptr;
    bool previewOk = previewSprite.createSprite(kPreviewSize, kPreviewSize) != nullptr;
    _renderReady = canvasOk && botOk && previewOk;
    if (!_renderReady) {
        Serial.printf("sprite allocation failed: canvas=%d bot=%d preview=%d\n", canvasOk, botOk, previewOk);
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.drawString("DISPLAY MEMORY ERROR", kW / 2, kH / 2);
        return;
    }

    face.begin(&botSprite);
    previewBot.begin(&previewSprite);
    applySettings();
    refreshPower(millis());
    Serial.printf("bot-ux-watch ready; IMU=%d. Commands: e/a/m/p/s, c capture, 0-4 preview screens\n",
                  M5.Imu.isEnabled());
}

void loop() {
    M5.update();
    uint32_t now = millis();
    if (!_renderReady) { delay(20); return; }

    refreshPower(now);
    updateMotion(now);
    handleInputs(now);
    handleSerialCommands();

    if (_screen != Screen::Face && !_uiDirty && !previewIsAnimated()) {
        delay(2);
        return;
    }
    uint32_t interval = _dozing ? kDozeFrameMs
                      : previewIsAnimated() ? kPreviewFrameMs : kActiveFrameMs;
    if (now - _lastFrameMs < interval) {
        delay(1);
        return;
    }
    _lastFrameMs = now;
    render(now);
}
