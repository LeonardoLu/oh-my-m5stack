// bot-ux-watch — a round-safe M5Stack StopWatch companion face.

#include <M5Unified.h>
#include <BotUx.h>

#include "CalendarMath.h"
#include "InputSemantics.h"
#include "Power.h"
#include "Settings.h"
#include "TimedState.h"
#include "WatchFace.h"
#include "WatchInteraction.h"
#include "WatchUi.h"
#include <UxInput.h>
#include <UxKeyboard.h>

#include <esp_heap_caps.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum class Screen : uint8_t { Face, Settings, Personalize, Editor };
enum class Editor : uint8_t { None, Time, Date, Format, Expression, Appearance, Motion, Color, Display, Name, Preview, Layout, Language };
enum class MenuItem : uint8_t { Time, Date, Format, Personalize, Display, Layout, Done, Count };
enum class PersonalItem : uint8_t { Expression, Action, Appearance, Color, Name, Language, Preview, Intensity, Speed, Back, Count };
using watchinput::Gesture;

namespace {
constexpr int16_t kW = 466;
constexpr int16_t kH = 466;
constexpr int16_t kBotSize = 286;
constexpr int16_t kBotX = (kW - kBotSize) / 2;
constexpr int16_t kBotY = 90;
constexpr int16_t kPreviewSize = 178;
constexpr int16_t kPreviewX = (kW - kPreviewSize) / 2;
constexpr int16_t kPreviewY = 62;
constexpr int16_t kMenuY = 105;
constexpr int16_t kMenuStep = 72;
constexpr uint8_t kVisibleRows = 4;
constexpr uint32_t kActiveFrameMs = 16;
constexpr uint32_t kPreviewFrameMs = 33;
constexpr uint32_t kDozeFrameMs = 250;
constexpr uint8_t kLowBattery = 15;

const char* const kMenuLabels[(uint8_t)MenuItem::Count] = {
    "TIME", "DATE", "FORMAT", "BOT", "DISPLAY", "LAYOUT", "DONE"
};
const char* const kPersonalLabels[(uint8_t)PersonalItem::Count] = {
    "EXPRESSION", "ACTION", "APPEARANCE", "COLOR", "NAME", "LANGUAGE", "COMBINATIONS", "INTENSITY", "SPEED", "BACK"
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
PersonalItem _personal = PersonalItem::Expression;
Settings::Data _originalSettings;
Screen _editorParent = Screen::Settings;
Screen _personalParent = Screen::Settings;

watchinput::ButtonGesture _buttonA;
watchinput::ButtonGesture _buttonB;
watchinput::TouchGesture _touch;
watchinteraction::SingleDoubleClick _bClick;
bool _listTouch = false;
bool _diagnosticScroll = false;
int _namePressedKey = -1;
watchinteraction::SettingsChord _settingsChord;
watchinteraction::ScrollList _settingsList;
watchinteraction::ScrollList _personalList;
ux::ScrollModel _settingsScroll, _personalScroll;
ux::NameEditor _nameEditor;
uint32_t _scrollMs = 0;
uint8_t _previewMood = 0, _previewExpression = 0, _previewAnimation = 0;
bool _manualPreset = false;
botux::BotUx::Preset _manualCombo{botux::BotUx::Mood::Idle, botux::BotUx::Expression::Auto, botux::BotUx::Animation::Auto};
uint32_t _gazeUntil = 0;
bool _originalManualPreset = false;
botux::BotUx::Preset _originalManualCombo = _manualCombo;
watchinteraction::MotionFilter _motionFilter;
watchinteraction::AmbientCycle _ambientCycle;
int16_t _tapX = 0, _tapY = 0;

uint8_t _battery = 100;
bool _charging = false;
bool _powerKnown = false;
uint32_t _powerReadMs = 0;
TimedState _happyAcknowledgment;
uint32_t _statusPanelStartMs = 0;
uint32_t _statusPanelUntilMs = 0;
bool _dozing = false;
bool _renderReady = false;
bool _uiDirty = true;
bool _listBandOnly = false;
bool _faceNeedsClear = true;
uint32_t _lastFrameMs = 0;
uint32_t _lastImuReadMs = 0;
botux::BotUx::Mood _ambientMood = botux::BotUx::Mood::Idle;
botux::BotUx::Mood _drawMood = (botux::BotUx::Mood)0xFF;
uint8_t _colorDrag = 0;
bool _gestureActive = false;

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

static bool tapHit(int16_t left, int16_t top, int16_t width, int16_t height) {
    return hit(_tapX, _tapY, left, top, width, height)
        && hit(_touch.startX(), _touch.startY(), left, top, width, height);
}

static void markListMoved() {
    if (!_uiDirty) _listBandOnly = true;
    _uiDirty = true;
}

static void revealRow(ux::ScrollModel& scroll, uint8_t index) {
    float top = index * kMenuStep;
    if (top < scroll.offset()) scroll.setOffset(top);
    else if (top + kMenuStep > scroll.offset() + 288) scroll.setOffset(top + kMenuStep - 288);
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
    face.bot().setName(settings.data().botName);
    previewBot.setName(settings.data().botName);
    face.setLayout(settings.data().swapLayout, settings.data().showDescription, settings.data().language);
    auto expression = (botux::BotUx::Expression)settings.data().expression;
    auto animation = (botux::BotUx::Animation)settings.data().animation;
    face.bot().setExpression(_manualPreset ? _manualCombo.expression : expression, 500);
    previewBot.setExpression(expression, 500);
    face.bot().setAnimation(_manualPreset ? _manualCombo.animation : animation);
    previewBot.setAnimation(animation);
    face.bot().setReducedMotion(!settings.data().motion);
    previewBot.setReducedMotion(!settings.data().motion);
    float amount = 0.25f + settings.data().motionAmount * 0.15f;
    float speed = 0.45f + settings.data().animationSpeed * 0.14f;
    face.bot().setMotionAmount(amount);
    previewBot.setMotionAmount(amount);
    face.bot().setAnimationSpeed(speed);
    previewBot.setAnimationSpeed(speed);
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
        _statusPanelStartMs = now;
        _statusPanelUntilMs = now + 6000;
    }
    _powerKnown = true;
}

static void showStatusPanel(uint32_t now, uint32_t duration = 6000) {
    _statusPanelStartMs = now;
    _statusPanelUntilMs = now + duration;
    _uiDirty = true;
}

static float statusPanelProgress(uint32_t now) {
    if (!_statusPanelUntilMs) return 0.0f;
    if ((int32_t)(now - _statusPanelUntilMs) >= 0) {
        _statusPanelUntilMs = 0;
        return 0.0f;
    }
    uint32_t age = now - _statusPanelStartMs;
    if (age < 300) return age / 300.0f;
    uint32_t left = _statusPanelUntilMs - now;
    if (left < 260) return left / 260.0f;
    return 1.0f;
}

static botux::BotUx::Mood faceMood(uint32_t now) {
    if (_dozing) return botux::BotUx::Mood::Sleepy;
    if (_happyAcknowledgment.active(now)) return botux::BotUx::Mood::Happy;
    if (_battery <= kLowBattery) return botux::BotUx::Mood::Sleepy;
    if (_screen != Screen::Face) return botux::BotUx::Mood::Listening;
    if (_manualPreset) return _manualCombo.mood;
    if ((int32_t)(_gazeUntil - now) > 0) return botux::BotUx::Mood::Idle;
    return settings.data().expression == 0 ? _ambientMood : botux::BotUx::Mood::Idle;
}

static void enterSettings() {
    _ambientCycle.postpone(millis());
    _screen = Screen::Settings;
    _editor = Editor::None;
    _menu = MenuItem::Time;
    _settingsList.configure((uint8_t)MenuItem::Count, kVisibleRows);
    _settingsList.select(0);
    _settingsScroll.setBounds((uint8_t)MenuItem::Count * kMenuStep, 288);
    _settingsScroll.setOffset(0);
    _uiDirty = true;
    confirmSound();
}

static void enterPersonalize(Screen parent = Screen::Settings) {
    _ambientCycle.postpone(millis());
    _personalParent = parent;
    _screen = Screen::Personalize;
    _editor = Editor::None;
    _personal = PersonalItem::Expression;
    _personalList.configure((uint8_t)PersonalItem::Count, kVisibleRows);
    _personalList.select(0);
    _personalScroll.setBounds((uint8_t)PersonalItem::Count * kMenuStep, 288);
    _personalScroll.setOffset(0);
    _uiDirty = true;
    confirmSound();
}

static void leavePersonalize() {
    _screen = _personalParent;
    _editor = Editor::None;
    _uiDirty = true;
    if (_screen == Screen::Face) {
        _faceNeedsClear = true;
        face.invalidate();
    }
    clickSound();
}

static void leaveSettings() {
    _screen = Screen::Face;
    _editor = Editor::None;
    settings.save();
    _faceNeedsClear = true;
    face.invalidate();
    confirmSound();
}

static void enterEditor(Editor editor, Screen parent = Screen::Settings) {
    _screen = Screen::Editor;
    _editor = editor;
    _originalSettings = settings.data();
    _originalManualPreset = _manualPreset; _originalManualCombo = _manualCombo;
    _editorParent = parent;
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
    _manualPreset = _originalManualPreset; _manualCombo = _originalManualCombo;
    settings.data() = _originalSettings;
    applySettings();
    _screen = _editorParent;
    _editor = Editor::None;
    _uiDirty = true;
    clickSound();
}

static void saveEditor() {
    if (_editor == Editor::Name) {
        _nameEditor.press(ux::NameEditor::Done);
        snprintf(settings.data().botName, sizeof(settings.data().botName), "%s", _nameEditor.text());
        applySettings();
    }
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
    _screen = _editorParent;
    _editor = Editor::None;
    _uiDirty = true;
    confirmSound();
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
    _manualPreset = false;
    settings.data().expression = (uint8_t)((settings.data().expression
        + Settings::EXPRESSION_COUNT + delta) % Settings::EXPRESSION_COUNT);
    auto expression = (botux::BotUx::Expression)settings.data().expression;
    face.bot().setAnimation((botux::BotUx::Animation)settings.data().animation);
    face.bot().setExpression(expression, 500);
    previewBot.setExpression(expression, 500);
    _uiDirty = true;
    if (persist) settings.save();
    clickSound();
}

static void updateMotion(uint32_t now) {
    if (now - _lastImuReadMs < 20) return;
    _lastImuReadMs = now;

    if (_dozing) {
        _motionFilter.reset();
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

    float gyroMagnitude = gyroOk ? sqrtf(gx * gx + gy * gy + gz * gz) : 0.0f;
    auto filtered = _motionFilter.update(accelOk ? ax : 0.0f, accelOk ? ay : 0.0f,
                                         gyroMagnitude, now);
    face.bot().setMotion(filtered.x, filtered.y, filtered.shake * 0.45f);
    previewBot.setMotion(filtered.x, filtered.y, filtered.shake * 0.45f);

    if (filtered.poke) {
        face.bot().poke();
        _ambientCycle.postpone(now);
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
    } else if (_editor == Editor::Layout) {
        if (_editField == 0) settings.data().showDescription = !settings.data().showDescription;
        else settings.data().swapLayout = !settings.data().swapLayout;
        applySettings();
    } else if (_editor == Editor::Language) {
        settings.data().language ^= 1; applySettings();
    } else if (_editor == Editor::Preview) {
        uint8_t* value = _editField == 0 ? &_previewMood : _editField == 1 ? &_previewExpression : &_previewAnimation;
        uint8_t count = _editField == 0 ? botux::BotUx::moodCount() : _editField == 1 ? botux::BotUx::expressionCount() : botux::BotUx::animationCount();
        *value = (*value + count + delta) % count;
    } else if (_editor == Editor::Expression) {
        cycleExpression(delta);
    } else if (_editor == Editor::Appearance) {
        if (_editField == 0) {
            settings.data().appearance = (uint8_t)((settings.data().appearance + Settings::APPEARANCE_COUNT + delta) % Settings::APPEARANCE_COUNT);
        } else {
            settings.data().eyeStyle = (uint8_t)((settings.data().eyeStyle + Settings::EYE_STYLE_COUNT + delta) % Settings::EYE_STYLE_COUNT);
        }
        applySettings();
    } else if (_editor == Editor::Motion) {
        if (_editField == 0) {
            _manualPreset = false;
            settings.data().animation = (uint8_t)((settings.data().animation
                + Settings::ANIMATION_COUNT + delta) % Settings::ANIMATION_COUNT);
        } else if (_editField == 1) {
            settings.data().motion = !settings.data().motion;
        } else if (_editField == 2) {
            int level = settings.data().motionAmount + delta;
            settings.data().motionAmount = (uint8_t)(level < 1 ? 1 : level > 5 ? 5 : level);
        } else {
            int level = settings.data().animationSpeed + delta;
            settings.data().animationSpeed = (uint8_t)(level < 1 ? 1 : level > 5 ? 5 : level);
        }
        applySettings();
    } else if (_editor == Editor::Display) {
        if (_editField == 0) {
            int level = settings.data().brightness + delta;
            settings.data().brightness = (uint8_t)(level < 1 ? 1 : level > 5 ? 5 : level);
        } else if (_editField == 1) {
            settings.data().theme = (uint8_t)((settings.data().theme + Settings::THEME_COUNT + delta) % Settings::THEME_COUNT);
        } else {
            settings.data().sound = !settings.data().sound;
        }
        applySettings();
    }
    clickSound();
}

static void selectMenuItem() {
    switch (_menu) {
        case MenuItem::Time:       enterEditor(Editor::Time); break;
        case MenuItem::Date:       enterEditor(Editor::Date); break;
        case MenuItem::Format:     enterEditor(Editor::Format); break;
        case MenuItem::Personalize: enterPersonalize(); break;
        case MenuItem::Display:    enterEditor(Editor::Display); break;
        case MenuItem::Layout:     enterEditor(Editor::Layout); break;
        case MenuItem::Done:       leaveSettings(); break;
        default: break;
    }
}

static void selectPersonalItem() {
    switch (_personal) {
        case PersonalItem::Expression: enterEditor(Editor::Expression, Screen::Personalize); break;
        case PersonalItem::Action:     enterEditor(Editor::Motion, Screen::Personalize); break;
        case PersonalItem::Appearance: enterEditor(Editor::Appearance, Screen::Personalize); break;
        case PersonalItem::Color:      enterEditor(Editor::Color, Screen::Personalize); break;
        case PersonalItem::Name: enterEditor(Editor::Name, Screen::Personalize); _nameEditor.begin(settings.data().botName); break;
        case PersonalItem::Language: enterEditor(Editor::Language, Screen::Personalize); break;
        case PersonalItem::Preview: enterEditor(Editor::Preview, Screen::Personalize); _previewMood = _previewExpression = _previewAnimation = 0; break;
        case PersonalItem::Intensity:  enterEditor(Editor::Motion, Screen::Personalize); _editField = 2; break;
        case PersonalItem::Speed:      enterEditor(Editor::Motion, Screen::Personalize); _editField = 3; break;
        case PersonalItem::Back:       leavePersonalize(); break;
        default: break;
    }
}

static void handleFaceInput(Gesture gesture) {
    if (gesture == Gesture::Tap) {
        if (_statusPanelUntilMs && tapHit(128, 0, 210, 72)) {
            _statusPanelUntilMs = 0;
        } else if (!_manualPreset || _manualCombo.mood == botux::BotUx::Mood::Idle) {
            _gazeUntil = millis() + 2000;
            face.bot().setMood(botux::BotUx::Mood::Idle);
            _drawMood = botux::BotUx::Mood::Idle;
            face.bot().gazeAt((_tapX - 233) / 180.0f, (_tapY - 233) / 180.0f);
        }
    } else if (gesture == Gesture::Long) {
        if (tapHit(kBotX, kBotY, kBotSize, kBotSize)) enterPersonalize(Screen::Face);
    } else if (gesture == Gesture::SwipeDown && _touch.startY() <= 20
               && _tapY - _touch.startY() > 50) {
        showStatusPanel(millis());
    }
}

static void handleSettingsTouch() {
    if (_tapY < 76 || _tapY >= 364) return;
    for (uint8_t i = 0; i < (uint8_t)MenuItem::Count; ++i) {
        int16_t cy = kMenuY + i * kMenuStep - (int16_t)_settingsScroll.offset();
        if (cy + 29 < 76 || cy - 29 >= 364) continue;
        if (tapHit(50, cy - 31, 366, 62)) {
            _settingsList.select(i);
            _menu = (MenuItem)_settingsList.selected();
            selectMenuItem();
            return;
        }
    }
}

static void handlePersonalTouch() {
    if (_tapY < 76 || _tapY >= 364) return;
    for (uint8_t i = 0; i < (uint8_t)PersonalItem::Count; ++i) {
        int16_t cy = kMenuY + i * kMenuStep - (int16_t)_personalScroll.offset();
        if (cy + 29 < 76 || cy - 29 >= 364) continue;
        if (tapHit(50, cy - 31, 366, 62)) {
            _personalList.select(i);
            _personal = (PersonalItem)_personalList.selected();
            selectPersonalItem();
            return;
        }
    }
}

static void updateColorPicker(int16_t x, int16_t y) {
    if (_colorDrag == 1) {
        if (x < 66) x = 66; if (x > 326) x = 326;
        if (y < 246) y = 246; if (y > 354) y = 354;
        settings.data().colorSat = (uint8_t)((x - 66) * 100 / 260);
        settings.data().colorValue = (uint8_t)((354 - y) * 100 / 108);
    } else if (_colorDrag == 2) {
        if (y < 246) y = 246; if (y > 354) y = 354;
        settings.data().colorHue = (uint16_t)((y - 246) * 359 / 108);
    } else return;
    settings.data().customColor = true;
    applySettings();
}

static void handleEditorTouch() {
    if (tapHit(84, 374, 138, 50)) { cancelEditor(); return; }
    if (tapHit(244, 374, 138, 50)) { saveEditor(); return; }

    if (_editor == Editor::Language) {
        if (tapHit(72, 160, 152, 70)) { settings.data().language = 0; applySettings(); }
        if (tapHit(242, 160, 152, 70)) { settings.data().language = 1; applySettings(); }
        return;
    }
    if (_editor == Editor::Layout || _editor == Editor::Preview) {
        const int16_t rows[] = { _editor == Editor::Layout ? (int16_t)170 : (int16_t)250,
                                _editor == Editor::Layout ? (int16_t)250 : (int16_t)296, 342 };
        uint8_t count = _editor == Editor::Layout ? 2 : 3;
        for (uint8_t i = 0; i < count; ++i) if (tapHit(58, rows[i] - 22, 350, 44)) {
            _editField = i; changeEditorValue(_tapX < 233 ? -1 : 1); return;
        }
        return;
    }
    if (_editor == Editor::Color && tapHit(163, 207, 140, 34)) {
        settings.data().customColor = false;
        applySettings();
        clickSound();
        return;
    }

    if (_editor == Editor::Time) {
        const int16_t centers[2] = { 157, 309 };
        for (uint8_t i = 0; i < 2; ++i) {
            if (tapHit(centers[i] - 50, 115, 100, 54)) { _editField = i; changeEditorValue(-1); return; }
            if (tapHit(centers[i] - 50, 255, 100, 54)) { _editField = i; changeEditorValue(1); return; }
            if (tapHit(centers[i] - 55, 175, 110, 72)) { _editField = i; clickSound(); return; }
        }
    } else if (_editor == Editor::Date) {
        const int16_t centers[3] = { 108, 233, 358 };
        for (uint8_t i = 0; i < 3; ++i) {
            if (tapHit(centers[i] - 43, 115, 86, 52)) { _editField = i; changeEditorValue(-1); return; }
            if (tapHit(centers[i] - 43, 255, 86, 52)) { _editField = i; changeEditorValue(1); return; }
            if (tapHit(centers[i] - 46, 175, 92, 72)) { _editField = i; clickSound(); return; }
        }
    } else if (_editor == Editor::Format) {
        if (tapHit(78, 128, 146, 76)) {
            _editField = 0;
            settings.data().hour24 = false; applySettings(); clickSound();
        } else if (tapHit(242, 128, 146, 76)) {
            _editField = 0;
            settings.data().hour24 = true; applySettings(); clickSound();
        } else if (tapHit(100, 242, 266, 58)) {
            _editField = 1;
            settings.data().showSeconds = !settings.data().showSeconds;
            applySettings(); clickSound();
        }
    } else if (_editor == Editor::Expression) {
        if (tapHit(48, 126, 76, 96)) cycleExpression(-1);
        else if (tapHit(342, 126, 76, 96)) cycleExpression(1);
        else if (tapHit(kPreviewX, kPreviewY, kPreviewSize, kPreviewSize)) {
            previewBot.poke(); pokeSound();
        }
    } else if (_editor == Editor::Appearance) {
        const int16_t rows[2] = { 278, 340 };
        for (uint8_t i = 0; i < 2; ++i) {
            if (tapHit(58, rows[i] - 23, 350, 46)) {
                _editField = i;
                int8_t delta = (_tapX < 233) ? -1 : 1;
                changeEditorValue(delta);
                return;
            }
        }
    } else if (_editor == Editor::Motion) {
        const int16_t rows[4] = { 218, 262, 306, 350 };
        for (uint8_t i = 0; i < 4; ++i) {
            if (tapHit(58, rows[i] - 24, 350, 48)) {
                _editField = i;
                int8_t delta = (_tapX < 233) ? -1 : 1;
                changeEditorValue(delta);
                return;
            }
        }
    } else if (_editor == Editor::Display) {
        const int16_t rows[3] = { 170, 250, 330 };
        for (uint8_t i = 0; i < 3; ++i) {
            if (tapHit(58, rows[i] - 30, 350, 60)) {
                _editField = i;
                changeEditorValue(_tapX < 233 ? -1 : 1);
                return;
            }
        }
    }
}

static void handleInputs(uint32_t now) {
    auto touch = M5.Touch.getDetail(0);
    _gestureActive = M5.BtnA.isPressed() || M5.BtnB.isPressed()
        || touch.wasPressed() || touch.isPressed();
    bool wakePressed = M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || touch.wasPressed();
    if (_dozing) {
        if (wakePressed) {
            _dozing = false;
            consumeWakeInput();
            power.applyLevel(settings.data().brightness);
        }
        return;
    }

    if (_screen == Screen::Face) {
        bool entered = _settingsChord.poll(M5.BtnA.isPressed(), M5.BtnB.isPressed(), now);
        if (_settingsChord.tracking()) {
            _bClick.cancel();
            _buttonA.consume(M5.BtnA.isPressed());
            _buttonB.consume(M5.BtnB.isPressed());
            if (entered) enterSettings();
            return;
        }
    }

    Gesture ga = _buttonA.poll(M5.BtnA.wasPressed(), M5.BtnA.isPressed(), M5.BtnA.wasReleased(), now);
    Gesture gb = _buttonB.poll(M5.BtnB.wasPressed(), M5.BtnB.isPressed(), M5.BtnB.wasReleased(), now);
    Gesture gt = _touch.poll(touch.wasPressed(), touch.isPressed(), touch.wasReleased(),
                             (int16_t)touch.x, (int16_t)touch.y, now);
    if (gt == Gesture::Tap) {
        _tapX = _touch.tapX();
        _tapY = _touch.tapY();
    } else if (gt != Gesture::None) {
        _tapX = (int16_t)touch.x;
        _tapY = (int16_t)touch.y;
    }

    ux::ScrollModel* scroll = _screen == Screen::Settings ? &_settingsScroll
        : _screen == Screen::Personalize ? &_personalScroll : nullptr;
    if (scroll) {
        if (touch.wasPressed()) {
            _listTouch = hit(touch.x, touch.y, 50, 76, 366, 288);
            if (_listTouch) scroll->begin(touch.y, now);
        }
        if (_listTouch && (touch.isPressed() || touch.wasReleased())) {
            float before = scroll->offset();
            scroll->move(touch.y, now);
            if (before != scroll->offset()) markListMoved();
        }
        if (touch.wasReleased()) {
            if (!_listTouch || !scroll->end(now)) gt = Gesture::None;
            _listTouch = false;
        }
        if (gt != Gesture::Tap) gt = Gesture::None;
    }
    if (_screen == Screen::Editor && _editor == Editor::Name) {
        const ux::Rect keys{78, 170, 310, 205};
        if (touch.wasPressed()) {
            _namePressedKey = ux::nameKeyAt(keys, touch.x, touch.y);
            _uiDirty = true;
        }
        if (touch.wasReleased() && gt == Gesture::Tap) {
            int key = ux::nameKeyAt(keys, _tapX, _tapY);
            if (key >= 0 && key == _namePressedKey) {
                bool done = _nameEditor.press(key);
                _uiDirty = true;
                if (done) {
                    snprintf(settings.data().botName, sizeof(settings.data().botName), "%s", _nameEditor.text());
                    applySettings(); saveEditor();
                }
                gt = Gesture::None;
            }
        }
        if (touch.wasReleased()) { _namePressedKey = -1; _uiDirty = true; }
    }

    if (_screen == Screen::Editor && _editor == Editor::Color) {
        if (touch.wasPressed()) {
            if (hit((int16_t)touch.x, (int16_t)touch.y, 66, 246, 260, 108)) _colorDrag = 1;
            else if (hit((int16_t)touch.x, (int16_t)touch.y, 344, 246, 56, 108)) _colorDrag = 2;
        }
        if (touch.isPressed() && _colorDrag) updateColorPicker((int16_t)touch.x, (int16_t)touch.y);
        if (touch.wasReleased()) _colorDrag = 0;
        if (gt == Gesture::SwipeUp || gt == Gesture::SwipeDown
            || gt == Gesture::SwipeLeft || gt == Gesture::SwipeRight || gt == Gesture::Long)
            gt = Gesture::None;
    }

    if (_screen == Screen::Face) {
        if (ga == Gesture::Tap) {
            _manualPreset = true;
            _manualCombo = botux::BotUx::preset(face.bot().randomPreset());
            _drawMood = face.bot().mood();
            face.invalidate(); clickSound();
        }
        if (gb == Gesture::Long) { _bClick.cancel(); startDoze(); }
        auto click = _bClick.poll(gb == Gesture::Tap, now);
        if (click == watchinteraction::SingleDoubleClick::Event::Single) {
            _manualPreset = true;
            _manualCombo = botux::BotUx::preset(face.bot().nextPreset());
            _drawMood = face.bot().mood();
            face.invalidate(); clickSound();
        } else if (click == watchinteraction::SingleDoubleClick::Event::Double) {
            _manualPreset = false;
            settings.data().expression = settings.data().animation = 0;
            face.bot().resetToIdle();
            _ambientMood = _drawMood = botux::BotUx::Mood::Idle;
            _ambientCycle.postpone(now);
            face.invalidate(); confirmSound();
        }
        if (gt != Gesture::None) handleFaceInput(gt);
        if (ga != Gesture::None || gb != Gesture::None || gt != Gesture::None) _ambientCycle.postpone(now);
        return;
    }
    _bClick.cancel();

    if (_screen == Screen::Settings) {
        if (ga == Gesture::Tap) selectMenuItem();
        else if (ga == Gesture::Long) leaveSettings();
        if (gb == Gesture::Tap) {
            _settingsList.move(1);
            _menu = (MenuItem)_settingsList.selected();
            revealRow(_settingsScroll, (uint8_t)_menu);
            _uiDirty = true;
            clickSound();
        }
        if (gt == Gesture::Tap) handleSettingsTouch();
        else if (gt == Gesture::SwipeUp || gt == Gesture::SwipeDown) {
            _settingsList.move(gt == Gesture::SwipeUp ? 1 : -1);
            _menu = (MenuItem)_settingsList.selected();
            revealRow(_settingsScroll, (uint8_t)_menu);
            _uiDirty = true;
        }
        return;
    }

    if (_screen == Screen::Personalize) {
        if (ga == Gesture::Long) leavePersonalize();
        if (gb == Gesture::Tap) {
            _personalList.move(1);
            _personal = (PersonalItem)_personalList.selected();
            revealRow(_personalScroll, (uint8_t)_personal);
            _uiDirty = true;
        }
        if (gt == Gesture::Tap) handlePersonalTouch();
        else if (gt == Gesture::SwipeUp || gt == Gesture::SwipeDown) {
            _personalList.move(gt == Gesture::SwipeUp ? 1 : -1);
            _personal = (PersonalItem)_personalList.selected();
            revealRow(_personalScroll, (uint8_t)_personal);
            _uiDirty = true;
        }
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
    ux::roundRect(canvas, x, y, w, h, radius, fill);
    ux::strokeRoundRect(canvas, x, y, w, h, radius,
                         selected ? settings.style().accentColor : settings.muted());
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(accentFill ? settings.style().bgColor : settings.ink());
    watchText(canvas, label, x + w / 2, y + h / 2);
}

static void drawTitle(const char* title) {
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.ink());
    watchText(canvas, title, kW / 2, 48);
}

static void drawSettingsList(bool chrome = true) {
    if (chrome) drawTitle("SETTINGS");
    canvas.setClipRect(50, 76, 366, 288);
    for (uint8_t i = 0; i < (uint8_t)MenuItem::Count; ++i) {
        int16_t cy = kMenuY + i * kMenuStep - (int16_t)_settingsScroll.offset();
        if (cy + 29 < 76 || cy - 29 >= 364) continue;
        bool selected = i == (uint8_t)_menu;
        ux::roundRect(canvas, 62, cy - 29, 342, 58, 25, selected ? settings.panel() : settings.style().bgColor);
        if (selected) ux::strokeRoundRect(canvas, 62, cy - 29, 342, 58, 25, settings.style().accentColor);

        canvas.setTextDatum(middle_left);
        canvas.setFont(&fonts::FreeSansBold12pt7b);
        canvas.setTextSize(1.0f);
        canvas.setTextColor(selected ? settings.style().accentColor : settings.ink());
        watchText(canvas, kMenuLabels[i], 88, cy);

        char value[24] = {};
        switch ((MenuItem)i) {
            case MenuItem::Time: snprintf(value, sizeof(value), "%02u:%02u", face.hour(), face.minute()); break;
            case MenuItem::Date: {
                snprintf(value, sizeof(value), "%04d/%02u/%02u", face.year(), face.month(), face.day());
                break;
            }
            case MenuItem::Format: snprintf(value, sizeof(value), settings.data().hour24 ? "24 H" : "12 H"); break;
            case MenuItem::Personalize: snprintf(value, sizeof(value), ">"); break;
            case MenuItem::Display: snprintf(value, sizeof(value), "%u / 5", settings.data().brightness); break;
            case MenuItem::Layout: snprintf(value, sizeof(value), settings.data().swapLayout ? "TIME TOP" : "BOT TOP"); break;
            default: break;
        }
        if (value[0]) {
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextDatum(middle_right);
            canvas.setTextColor(settings.muted());
            watchText(canvas, value, 378, cy);
        }
    }
    canvas.clearClipRect();
    if (chrome) ux::roundRect(canvas, 221, 410, 24, 5, 2.5f, settings.muted());
}

static void drawPersonalizeList(bool chrome = true) {
    if (chrome) drawTitle("BOT PERSONALITY");
    canvas.setClipRect(50, 76, 366, 288);
    for (uint8_t i = 0; i < (uint8_t)PersonalItem::Count; ++i) {
        int16_t cy = kMenuY + i * kMenuStep - (int16_t)_personalScroll.offset();
        if (cy + 29 < 76 || cy - 29 >= 364) continue;
        bool selected = i == (uint8_t)_personal;
        ux::roundRect(canvas, 62, cy - 29, 342, 58, 25, selected ? settings.panel() : settings.style().bgColor);
        if (selected) ux::strokeRoundRect(canvas, 62, cy - 29, 342, 58, 25, settings.style().accentColor);
        canvas.setFont(&fonts::FreeSansBold12pt7b);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(selected ? settings.style().accentColor : settings.ink());
        watchText(canvas, kPersonalLabels[i], 78, cy);
        char value[20] = {};
        switch ((PersonalItem)i) {
            case PersonalItem::Expression: snprintf(value, sizeof(value), "%s", Settings::expressionName(settings.data().expression)); break;
            case PersonalItem::Action: snprintf(value, sizeof(value), "%s", Settings::animationName(settings.data().animation)); break;
            case PersonalItem::Appearance: snprintf(value, sizeof(value), "%s", Settings::appearanceName(settings.data().appearance)); break;
            case PersonalItem::Color: snprintf(value, sizeof(value), settings.data().customColor ? "CUSTOM" : "THEME"); break;
            case PersonalItem::Name: snprintf(value, sizeof(value), "%s", settings.data().botName); break;
            case PersonalItem::Language: snprintf(value, sizeof(value), settings.data().language ? "中文" : "English"); break;
            case PersonalItem::Preview: snprintf(value, sizeof(value), "960"); break;
            case PersonalItem::Intensity: snprintf(value, sizeof(value), "%u / 5", settings.data().motionAmount); break;
            case PersonalItem::Speed: snprintf(value, sizeof(value), "%u / 5", settings.data().animationSpeed); break;
            default: break;
        }
        if (value[0]) {
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextDatum(middle_right);
            canvas.setTextColor(settings.muted());
            watchText(canvas, value, 390, cy);
        }
    }
    canvas.clearClipRect();
    if (chrome) ux::roundRect(canvas, 221, 410, 24, 5, 2.5f, settings.muted());
}

static void drawFooter() {
    drawPill(84, 376, 138, 46, "BACK", false);
    drawPill(244, 376, 138, 46, "DONE", true, true);
}

static void drawStepper(int16_t cx, const char* label, const char* value, bool selected, int16_t width) {
    drawPill(cx - width / 2, 116, width, 52, "-", selected);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted());
    watchText(canvas, label, cx, 190);
    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(selected ? settings.style().accentColor : settings.ink());
    watchText(canvas, value, cx, 220);
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
    watchText(canvas, ":", 233, 220);
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
    watchText(canvas, "Choose whether seconds appear in the clock", 233, 330);
    drawFooter();
}

static void drawArrowRow(int16_t cy, const char* label, const char* value, bool selected) {
    if (selected) ux::roundRect(canvas, 58, cy - 22, 350, 44, 17, settings.panel());
    canvas.setTextDatum(middle_left);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted());
    watchText(canvas, label, 76, cy);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(settings.ink());
    watchText(canvas, value, 270, cy);
    canvas.setTextColor(settings.style().accentColor);
    watchText(canvas, "<", 205, cy);
    watchText(canvas, ">", 388, cy);
}

static void drawAppearanceEditor() {
    drawTitle("APPEARANCE");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, kPreviewY);
    drawArrowRow(278, "SHAPE", Settings::appearanceName(settings.data().appearance), _editField == 0);
    drawArrowRow(340, "EYES", Settings::eyeStyleName(settings.data().eyeStyle), _editField == 1);
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
    watchText(canvas, Settings::expressionName(settings.data().expression), 233, 274);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(settings.muted());
    watchText(canvas, "Swipe or tap arrows; tap the bot to react", 233, 326);
    drawFooter();
}

static void drawMotionEditor() {
    drawTitle("MOTION");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, kPreviewY);
    char amount[8], speed[8];
    snprintf(amount, sizeof(amount), "%u / 5", settings.data().motionAmount);
    snprintf(speed, sizeof(speed), "%u / 5", settings.data().animationSpeed);
    drawArrowRow(218, "ACTION", Settings::animationName(settings.data().animation), _editField == 0);
    drawArrowRow(262, "WRIST", settings.data().motion ? "ON" : "OFF", _editField == 1);
    drawArrowRow(306, "INTENSITY", amount, _editField == 2);
    drawArrowRow(350, "SPEED", speed, _editField == 3);
    drawFooter();
}

static void drawColorEditor() {
    drawTitle("BOT COLOR");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, 55);
    drawPill(163, 207, 140, 32, "USE THEME", !settings.data().customColor);
    for (int16_t x = 66; x < 326; x += 10) {
        uint8_t sat = (uint8_t)((x - 66) * 100 / 260);
        for (int16_t y = 246; y < 354; y += 6) {
            uint8_t value = (uint8_t)((354 - y) * 100 / 108);
            canvas.fillRect(x, y, 11, 7, Settings::hsv565(settings.data().colorHue, sat, value));
        }
    }
    for (int16_t y = 246; y < 354; y += 3)
        canvas.fillRect(344, y, 56, 4, Settings::hsv565((uint16_t)((y - 246) * 359 / 108), 100, 100));
    int16_t sx = 66 + settings.data().colorSat * 260 / 100;
    int16_t sy = 354 - settings.data().colorValue * 108 / 100;
    int16_t hy = 246 + settings.data().colorHue * 108 / 359;
    canvas.drawCircle(sx, sy, 8, settings.ink());
    ux::strokeRoundRect(canvas, 340, hy - 5, 64, 10, 4, settings.ink());
    drawFooter();
}

static void drawDisplayEditor() {
    drawTitle("DISPLAY & SOUND");
    char brightness[8];
    snprintf(brightness, sizeof(brightness), "%u / 5", settings.data().brightness);
    drawArrowRow(170, "BRIGHTNESS", brightness, _editField == 0);
    drawArrowRow(250, "THEME", Settings::themeName(settings.data().theme), _editField == 1);
    drawArrowRow(330, "SOUND", settings.data().sound ? "ON" : "OFF", _editField == 2);
    drawFooter();
}

static void drawNameEditor() {
    drawTitle("BOT NAME");
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextColor(settings.ink());
    watchText(canvas, _nameEditor.text(), 233, 128);
    ux::drawNameKeyboard(canvas, _nameEditor, ux::Rect{78,170,310,205},
                         settings.panel(), settings.ink(), ux::Latin14, _namePressedKey);
    drawFooter();
}

static void drawLanguageEditor() {
    drawTitle("LANGUAGE");
    drawPill(72, 160, 152, 70, "English", settings.data().language == 0);
    drawPill(242, 160, 152, 70, "中文", settings.data().language == 1);
    drawFooter();
}

static void drawLayoutEditor() {
    drawTitle("WATCH LAYOUT");
    drawArrowRow(170, "BOT TEXT", settings.data().showDescription ? "SHOW" : "HIDE", _editField == 0);
    drawArrowRow(250, "TOP", settings.data().swapLayout ? "TIME" : "BOT TEXT", _editField == 1);
    drawFooter();
}

static void drawCombinationEditor() {
    drawTitle("COMBINATIONS");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, 62);
    drawArrowRow(250, "STATE", botux::BotUx::moodName((botux::BotUx::Mood)_previewMood), _editField == 0);
    drawArrowRow(296, "FACE", botux::BotUx::expressionName((botux::BotUx::Expression)_previewExpression), _editField == 1);
    drawArrowRow(342, "ACTION", botux::BotUx::animationName((botux::BotUx::Animation)_previewAnimation), _editField == 2);
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
        case Editor::Color:      drawColorEditor(); break;
        case Editor::Display:    drawDisplayEditor(); break;
        case Editor::Name: drawNameEditor(); break;
        case Editor::Preview: drawCombinationEditor(); break;
        case Editor::Layout: drawLayoutEditor(); break;
        case Editor::Language: drawLanguageEditor(); break;
        default: break;
    }
}

static bool previewIsAnimated() {
    return _screen == Screen::Editor
        && (_editor == Editor::Preview || _editor == Editor::Expression || _editor == Editor::Appearance
            || _editor == Editor::Motion || _editor == Editor::Color);
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
    auto mood = faceMood(now);
    if (mood != _drawMood) {
        face.bot().setMood(mood, 700);
        _drawMood = mood;
    }
    auto selectedExpression = (botux::BotUx::Expression)settings.data().expression;
    if (!_manualPreset) face.bot().setExpression((_dozing || _battery <= kLowBattery)
        ? botux::BotUx::Expression::Auto : selectedExpression);
    face.update(now);
    face.bot().update(now);

    if (previewIsAnimated()) {
        if (_editor == Editor::Preview) {
            previewBot.setMood((botux::BotUx::Mood)_previewMood);
            previewBot.setTalking(_previewMood == (uint8_t)botux::BotUx::Mood::Speaking);
            previewBot.setExpression((botux::BotUx::Expression)_previewExpression);
            previewBot.setAnimation((botux::BotUx::Animation)_previewAnimation);
        } else {
            previewBot.setTalking(false);
            previewBot.setMood(botux::BotUx::Mood::Listening);
            previewBot.setExpression((botux::BotUx::Expression)settings.data().expression);
            previewBot.setAnimation((botux::BotUx::Animation)settings.data().animation);
        }
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
                  statusPanelProgress(now));
        M5.Display.waitDisplay();
        uint32_t t3 = micros();
        recordFrame(now, t1 - t0, t2 - t1, t3 - t2);
        return;
    }

    if (!_uiDirty && !previewIsAnimated()) return;
    bool bandOnly = _listBandOnly && (_screen == Screen::Settings || _screen == Screen::Personalize);
    if (bandOnly) canvas.fillRect(0, 76, kW, 288, settings.style().bgColor);
    else canvas.fillSprite(settings.style().bgColor);
    if (_screen == Screen::Settings) {
        drawSettingsList(!bandOnly);
    } else if (_screen == Screen::Personalize) {
        drawPersonalizeList(!bandOnly);
    } else {
        drawEditor();
    }
    uint32_t t2 = micros();
    if (bandOnly) M5.Display.pushImage(0, 76, kW, 288, (uint16_t*)canvas.getBuffer() + kW*76);
    else canvas.pushSprite(0, 0);
    M5.Display.waitDisplay();
    uint32_t t3 = micros();
    _uiDirty = false;
    _listBandOnly = false;
    recordFrame(now, t1 - t0, t2 - t1, t3 - t2);
}

static void emitFrameCapture() {
    if (_screen == Screen::Face) {
        canvas.fillSprite(settings.style().bgColor);
        botSprite.pushSprite(&canvas, kBotX, kBotY);
        face.invalidate();
        face.draw(&canvas, settings.style().bgColor, settings.ink(), settings.muted(),
                  settings.style().accentColor, settings.panel(), settings.warning(),
                  statusPanelProgress(millis()));
    } else {
        canvas.fillSprite(settings.style().bgColor);
        if (_screen == Screen::Settings) drawSettingsList();
        else if (_screen == Screen::Personalize) drawPersonalizeList();
        else drawEditor();
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

static void selectDiagnosticPage(uint8_t page) {
    _diagnosticScroll = page == 19;
    static bool override = false;
    static Settings::Data prior;
    if (override) { settings.data() = prior; applySettings(); override = false; }
    if (page == 16 || page == 17) {
        prior = settings.data(); override = true;
        settings.data().language = page == 16 ? 1 : 0;
        settings.data().swapLayout = page == 17;
        settings.data().showDescription = true;
        applySettings();
    }
    _originalSettings = settings.data();
    _editor = Editor::None;
    _editorParent = Screen::Personalize;
    _personalParent = Screen::Settings;
    if (page == 0 || page == 10 || page == 16 || page == 17) {
        _screen = Screen::Face;
        _faceNeedsClear = true;
        face.invalidate();
        if (page == 10) {
            showStatusPanel(millis(), 6000);
            _statusPanelStartMs -= 300;
        }
    } else if (page == 1 || page == 8 || page == 18 || page == 19) {
        _screen = Screen::Settings;
        _settingsList.configure((uint8_t)MenuItem::Count, kVisibleRows);
        _settingsList.select(page == 8 ? (uint8_t)MenuItem::Done : 0);
        _menu = (MenuItem)_settingsList.selected();
        _settingsScroll.setBounds((uint8_t)MenuItem::Count*kMenuStep,288);
        _settingsScroll.setOffset(page == 8 ? 9999 : page == 18 ? 37 : 0);
    } else if (page == 2 || page == 9) {
        _screen = Screen::Personalize;
        _personalList.configure((uint8_t)PersonalItem::Count, kVisibleRows);
        _personalList.select(page == 9 ? (uint8_t)PersonalItem::Back : 0);
        _personal = (PersonalItem)_personalList.selected();
        _personalScroll.setBounds((uint8_t)PersonalItem::Count*kMenuStep,288);
        _personalScroll.setOffset(page == 9 ? 9999 : 0);
    } else {
        _screen = Screen::Editor;
        _editor = page == 3 ? Editor::Expression
                : page == 4 ? Editor::Appearance
                : page == 5 ? Editor::Motion
                : page == 6 ? Editor::Color
                : page == 7 ? Editor::Display
                : page == 11 ? Editor::Format
                : page == 12 ? Editor::Name
                : (page == 13 || page == 20) ? Editor::Preview
                : page == 14 ? Editor::Layout
                : page == 15 ? Editor::Language : Editor::Time;
        if (page == 12) _nameEditor.begin(settings.data().botName);
        if (page == 20) {
            _previewMood = (uint8_t)botux::BotUx::Mood::Happy;
            _previewExpression = (uint8_t)botux::BotUx::Expression::Joy;
            _previewAnimation = (uint8_t)botux::BotUx::Animation::Wave;
        }
        _editField = 0;
    }
    _uiDirty = true;
}

static void handleSerialCommands() {
    static bool readingView = false;
    static uint8_t view = 0;
    while (Serial.available()) {
        char command = (char)Serial.read();
        if (readingView) {
            if (command >= '0' && command <= '9') {
                view = (uint8_t)(view * 10 + command - '0');
                continue;
            }
            selectDiagnosticPage(view);
            readingView = false;
            if (command == '\n' || command == '\r') continue;
        }
        if (command == 'v') {
            readingView = true;
            view = 0;
            continue;
        }
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
        else if (command == 's') showStatusPanel(millis());
        else if (command == 'c') emitFrameCapture();
        else if (command >= '0' && command <= '9') selectDiagnosticPage((uint8_t)(command - '0'));
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

    _renderReady = face.begin(&botSprite);
    if (!_renderReady) { Serial.println("HUD allocation failed"); return; }
    previewBot.begin(&previewSprite);
    applySettings();
    refreshPower(millis());
    _settingsList.configure((uint8_t)MenuItem::Count, kVisibleRows);
    _personalList.configure((uint8_t)PersonalItem::Count, kVisibleRows);
    _ambientCycle.begin(millis());
    showStatusPanel(millis(), 1800);
    Serial.printf("bot-ux-watch ready; IMU=%d. Commands: e/a/m/p/s, c capture, v0..v20\\n diagnostic pages\n",
                  M5.Imu.isEnabled());
}

void loop() {
    M5.update();
    uint32_t now = millis();
    if (!_renderReady) { delay(20); return; }

    refreshPower(now);
    updateMotion(now);
    handleInputs(now);
    if (_screen == Screen::Face && !_manualPreset && !_gestureActive
        && settings.data().expression == 0 && _ambientCycle.update(now)) {
        static const botux::BotUx::Mood moods[6] = {
            botux::BotUx::Mood::Idle, botux::BotUx::Mood::Listening,
            botux::BotUx::Mood::Thinking, botux::BotUx::Mood::Done,
            botux::BotUx::Mood::Happy, botux::BotUx::Mood::Waiting,
        };
        _ambientMood = moods[_ambientCycle.index()];
    }
    handleSerialCommands();
    if (_diagnosticScroll && _screen == Screen::Settings) {
        _settingsScroll.setOffset((sinf(now * 0.001f) + 1) * 0.5f * ((uint8_t)MenuItem::Count*kMenuStep - 288));
        markListMoved();
    }
    uint32_t scrollDt = _scrollMs ? now - _scrollMs : 0; _scrollMs = now;
    if (_screen == Screen::Settings && _settingsScroll.update(scrollDt)) markListMoved();
    if (_screen == Screen::Personalize && _personalScroll.update(scrollDt)) markListMoved();

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
