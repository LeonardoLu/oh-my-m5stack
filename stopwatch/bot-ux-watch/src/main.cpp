// bot-ux-watch — a round-safe M5Stack StopWatch companion face.

#include <M5Unified.h>
#include <BotUx.h>

#include "CalendarMath.h"
#include "InputSemantics.h"
#include "Power.h"
#include "Settings.h"
#include "TimedState.h"
#include "WatchFace.h"

#include <stdio.h>
#include <string.h>

enum class Screen : uint8_t { Face, Settings, Editor };
enum class Editor : uint8_t { None, Time, Date, Format, Appearance, Brightness };
enum class MenuItem : uint8_t { Time, Date, Format, Appearance, Brightness, Done, Count };
using watchinput::Gesture;

namespace {
constexpr int16_t kW = 466;
constexpr int16_t kH = 466;
constexpr int16_t kBotSize = 206;
constexpr int16_t kBotX = (kW - kBotSize) / 2;
constexpr int16_t kBotY = 64;
constexpr int16_t kPreviewSize = 124;
constexpr int16_t kPreviewX = (kW - kPreviewSize) / 2;
constexpr int16_t kPreviewY = 66;
constexpr int16_t kMenuY = 94;
constexpr int16_t kMenuStep = 54;
constexpr uint32_t kActiveFrameMs = 33;
constexpr uint32_t kDozeFrameMs = 250;
constexpr uint8_t kLowBattery = 15;

const char* const kMenuLabels[(uint8_t)MenuItem::Count] = {
    "TIME", "DATE", "FORMAT", "APPEARANCE", "BRIGHTNESS", "DONE"
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
bool _dozing = false;
bool _renderReady = false;
uint32_t _lastFrameMs = 0;

uint8_t _editHour = 0;
uint8_t _editMinute = 0;
int16_t _editYear = 2026;
uint8_t _editMonth = 1;
uint8_t _editDay = 1;
uint8_t _editField = 0;

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
    face.setHour24(settings.data().hour24);
    face.setShowSeconds(settings.data().showSeconds);
    power.applyLevel(settings.data().brightness);
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
    confirmSound();
}

static void leaveSettings() {
    _screen = Screen::Face;
    _editor = Editor::None;
    settings.save();
    confirmSound();
}

static void enterEditor(Editor editor) {
    _screen = Screen::Editor;
    _editor = editor;
    _originalSettings = settings.data();
    _editField = 0;
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
    confirmSound();
}

static void toggleSeconds() {
    settings.data().showSeconds = !settings.data().showSeconds;
    face.setShowSeconds(settings.data().showSeconds);
    settings.save();
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
        settings.data().hour24 = !settings.data().hour24;
        applySettings();
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
        case MenuItem::Appearance: enterEditor(Editor::Appearance); break;
        case MenuItem::Brightness: enterEditor(Editor::Brightness); break;
        case MenuItem::Done:       leaveSettings(); break;
        default: break;
    }
}

static void handleFaceInput(Gesture gesture) {
    if (gesture == Gesture::Tap) {
        if (hit(_tapX, _tapY, 154, 378, 158, 70)) enterSettings();
        else if (hit(_tapX, _tapY, 90, 282, 286, 98)) toggleSeconds();
        else if (hit(_tapX, _tapY, kBotX, kBotY, kBotSize, kBotSize)) pokeBot();
    } else if (gesture == Gesture::SwipeUp) {
        enterSettings();
    } else if (gesture == Gesture::SwipeDown) {
        startDoze();
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
    if (hit(_tapX, _tapY, 84, 368, 138, 54)) { cancelEditor(); return; }
    if (hit(_tapX, _tapY, 244, 368, 138, 54)) { saveEditor(); return; }

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
        if (hit(_tapX, _tapY, 78, 154, 146, 84)) {
            settings.data().hour24 = false; applySettings(); clickSound();
        } else if (hit(_tapX, _tapY, 242, 154, 146, 84)) {
            settings.data().hour24 = true; applySettings(); clickSound();
        }
    } else if (_editor == Editor::Appearance) {
        const int16_t rows[3] = { 226, 282, 338 };
        for (uint8_t i = 0; i < 3; ++i) {
            if (hit(_tapX, _tapY, 58, rows[i] - 23, 350, 46)) {
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
        if (gb == Gesture::Tap) toggleSeconds();
        else if (gb == Gesture::Long) startDoze();
        if (gt != Gesture::None) handleFaceInput(gt);
        return;
    }

    if (_screen == Screen::Settings) {
        if (ga == Gesture::Tap) selectMenuItem();
        else if (ga == Gesture::Long) leaveSettings();
        if (gb == Gesture::Tap) {
            _menu = (MenuItem)(((uint8_t)_menu + 1) % (uint8_t)MenuItem::Count);
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
}

static void drawPill(int16_t x, int16_t y, int16_t w, int16_t h,
                     const char* label, bool selected, bool accentFill = false) {
    uint16_t fill = accentFill ? settings.style().accentColor : settings.panel();
    canvas.fillRoundRect(x, y, w, h, h / 2, fill);
    canvas.drawRoundRect(x, y, w, h, h / 2,
                         selected ? settings.style().accentColor : settings.muted());
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(1.35f);
    canvas.setTextColor(accentFill ? settings.style().bgColor : settings.ink());
    canvas.drawString(label, x + w / 2, y + h / 2);
}

static void drawTitle(const char* title) {
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(1.55f);
    canvas.setTextColor(settings.ink());
    canvas.drawString(title, kW / 2, 48);
}

static void drawSettingsList() {
    drawTitle("SETTINGS");
    for (uint8_t i = 0; i < (uint8_t)MenuItem::Count; ++i) {
        int16_t cy = kMenuY + i * kMenuStep;
        bool selected = i == (uint8_t)_menu;
        if (selected) canvas.fillRoundRect(78, cy - 22, 310, 44, 18, settings.panel());

        canvas.setTextDatum(middle_left);
        canvas.setTextSize(1.3f);
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
            case MenuItem::Appearance: snprintf(value, sizeof(value), "%s", Settings::appearanceName(settings.data().appearance)); break;
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
    canvas.setTextSize(0.9f);
    canvas.setTextColor(settings.muted());
    canvas.drawString("A SELECT     B MOVE", 233, 425);
}

static void drawFooter() {
    drawPill(84, 370, 138, 50, "BACK", false);
    drawPill(244, 370, 138, 50, "DONE", true, true);
}

static void drawStepper(int16_t cx, const char* label, const char* value, bool selected, int16_t width) {
    drawPill(cx - width / 2, 116, width, 52, "-", selected);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted());
    canvas.drawString(label, cx, 190);
    canvas.setTextSize(2.5f);
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
    canvas.setTextSize(2.0f);
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
    drawPill(78, 154, 146, 84, "12 HOUR", !settings.data().hour24, !settings.data().hour24);
    drawPill(242, 154, 146, 84, "24 HOUR", settings.data().hour24, settings.data().hour24);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted());
    canvas.drawString("Tap the clock face to show seconds", 233, 286);
    drawFooter();
}

static void drawArrowRow(int16_t cy, const char* label, const char* value, bool selected) {
    if (selected) canvas.fillRoundRect(58, cy - 22, 350, 44, 17, settings.panel());
    canvas.setTextDatum(middle_left);
    canvas.setTextSize(1.1f);
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
    drawArrowRow(226, "THEME", Settings::themeName(settings.data().theme), _editField == 0);
    drawArrowRow(282, "SHAPE", Settings::appearanceName(settings.data().appearance), _editField == 1);
    drawArrowRow(338, "SOUND", settings.data().sound ? "ON" : "OFF", _editField == 2);
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
    canvas.setTextSize(1.4f);
    canvas.setTextColor(settings.ink());
    canvas.drawString(value, 233, 278);
    drawFooter();
}

static void drawEditor() {
    switch (_editor) {
        case Editor::Time:       drawTimeEditor(); break;
        case Editor::Date:       drawDateEditor(); break;
        case Editor::Format:     drawFormatEditor(); break;
        case Editor::Appearance: drawAppearanceEditor(); break;
        case Editor::Brightness: drawBrightnessEditor(); break;
        default: break;
    }
}

static void render(uint32_t now) {
    canvas.fillSprite(settings.style().bgColor);
    face.setBattery(_battery);
    face.setCharging(_charging);
    face.setHour24(settings.data().hour24);
    face.setShowSeconds(settings.data().showSeconds);
    face.bot().setMood(faceMood(now));
    face.update(now);
    face.bot().update(now);

    if (_editor == Editor::Appearance) {
        previewBot.setMood(botux::BotUx::Mood::Listening);
        previewBot.update(now);
    }

    if (_screen == Screen::Face) {
        face.bot().draw();
        botSprite.pushSprite(&canvas, kBotX, kBotY);
        face.draw(settings.ink(), settings.muted(), settings.style().accentColor,
                  settings.panel(), settings.warning());
    } else if (_screen == Screen::Settings) {
        drawSettingsList();
    } else {
        drawEditor();
    }

    canvas.pushSprite(0, 0);
    M5.Display.waitDisplay();
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    settings.begin();
    power.begin();
    seedRtcIfNeeded();

    canvas.setColorDepth(16);
    botSprite.setColorDepth(16);
    previewSprite.setColorDepth(16);
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

    face.begin(&canvas, &botSprite);
    previewBot.begin(&previewSprite);
    applySettings();
    refreshPower(millis());
}

void loop() {
    M5.update();
    uint32_t now = millis();
    if (!_renderReady) { delay(20); return; }

    refreshPower(now);
    handleInputs(now);

    uint32_t interval = _dozing ? kDozeFrameMs : kActiveFrameMs;
    if (now - _lastFrameMs < interval) {
        delay(1);
        return;
    }
    _lastFrameMs = now;
    render(now);
}
