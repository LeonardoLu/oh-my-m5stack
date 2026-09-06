// bot-ux-watch — a round-safe M5Stack StopWatch companion face.

#include <M5Unified.h>
#include <BotUx.h>

#include "CalendarMath.h"
#include "CompanionPresets.h"
#include "InputSemantics.h"
#include "TouchContact.h"
#include "Power.h"
#include "Settings.h"
#include "TimedState.h"
#include "WatchFace.h"
#include "WatchInteraction.h"
#include "WatchUi.h"
#include "WatchControls.h"
#include <UxPointer.h>
#include <UxSound.h>
#include <UxSoundM5.h>
#include <UxInput.h>
#include <UxKeyboard.h>

#include <esp_heap_caps.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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
constexpr int16_t kMenuStep = 72;
constexpr uint8_t kVisibleRows = 4;
constexpr uint8_t kMenuCount=(uint8_t)MenuItem::Done;
constexpr uint8_t kPersonalCount=(uint8_t)PersonalItem::Back;
constexpr uint32_t kActiveFrameMs = 16;
constexpr uint32_t kPreviewFrameMs = 33;
constexpr uint32_t kDozeFrameMs = 250;
constexpr uint8_t kLowBattery = 15;

const char* const kMenuLabels[(uint8_t)MenuItem::Count] = {
    "TIME", "DATE", "FORMAT", "BOT", "DISPLAY", "LAYOUT", "DONE"
};
const char* const kPersonalLabels[(uint8_t)PersonalItem::Count] = {
    "EXPRESSION", "ACTION", "APPEARANCE", "COLOR", "NAME", "LANGUAGE", "COMBINATIONS", "GAZE", "INTENSITY", "SPEED", "BACK"
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
watchinput::TouchContact _contact;
watchinput::RegionDoubleTap _clockDoubleTap;
uint32_t _contactReadMs=0, _gazeTouchMs=0;
bool _faceTracking=false;
bool _diagnosticContact=false, _diagnosticDown=false, _contactReplyPending=false;
int16_t _diagnosticX=0, _diagnosticY=0;
watchinteraction::SingleDoubleClick _bClick;
ux::PointerSession _pointer;
ux::Rect _clickRect{0,0,0,0};
uint32_t _clickUntil=0;
int _capturedTarget=watchcontrols::None;
bool _soundReady=false;
ux::sound::Synth _sounds;
ux::sound::M5Output<m5::Speaker_Class> _soundOutput;
bool _diagnosticScroll = false;
int _namePressedKey = -1;
watchinteraction::SettingsChord _settingsChord;
watchinteraction::ScrollList _settingsList;
watchinteraction::ScrollList _personalList;
ux::ScrollModel _settingsScroll, _personalScroll;
ux::ScrollModel _editorScroll;
ux::NameEditor _nameEditor;
uint32_t _scrollMs = 0;
uint8_t _previewMood = 0, _previewExpression = 0, _previewAnimation = 0;
bool _manualPreset = false;
botux::BotUx::Preset _manualCombo{botux::BotUx::Mood::Idle, botux::BotUx::Expression::Auto, botux::BotUx::Animation::Auto};
watchcompanion::MoodDeck _moodDeck(botux::BotUx::moodCount());
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
bool _buttonNavigation = false;

enum class TouchTraceKind : uint8_t { Raw, Down, Scroll, End, Screen };
enum class TouchEndReason : uint8_t { Accepted, NoTarget, Scrolled, TargetChanged, Timeout, Outside };
struct TouchTraceEvent {
    uint32_t atMs;
    int16_t x, y;
    int16_t target, other;
    uint16_t elapsedMs, sampleGapMs, readUs;
    uint8_t kind, screen, flags;
};
constexpr uint16_t kTouchTraceCapacity = 160;
TouchTraceEvent _touchTrace[kTouchTraceCapacity];
uint16_t _touchTraceHead = 0, _touchTraceCount = 0;
uint32_t _touchTraceDropped = 0, _traceLastHeldMs = 0, _traceLastDelayMs = 0, _traceAcquireCount = 0;
uint16_t _traceMaxSampleGapMs = 0, _traceMaxReadUs = 0;
volatile bool _touchTraceEnabled = false;
bool _traceRawKnown = false, _traceRawContact = false;
bool _touchIrqAttached = false;
constexpr uint8_t kTouchIrqCapacity = 64;
volatile uint32_t _touchIrqTimes[kTouchIrqCapacity];
volatile uint32_t _touchIrqTotal = 0, _touchIrqDropped = 0;
volatile uint8_t _touchIrqHead = 0, _touchIrqCount = 0;

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

static uint16_t traceClamp16(uint32_t value) { return value > 65535 ? 65535 : (uint16_t)value; }

static void IRAM_ATTR touchTraceIrq() {
    if (!_touchTraceEnabled) return;
    uint8_t index = (_touchIrqHead + _touchIrqCount) % kTouchIrqCapacity;
    if (_touchIrqCount == kTouchIrqCapacity) {
        index = _touchIrqHead;
        _touchIrqHead = (_touchIrqHead + 1) % kTouchIrqCapacity;
        ++_touchIrqDropped;
    } else {
        ++_touchIrqCount;
    }
    _touchIrqTimes[index] = micros();
    ++_touchIrqTotal;
}

static void pushTouchTrace(TouchTraceKind kind, uint32_t atMs, int16_t x, int16_t y,
                           int16_t target = watchcontrols::None, int16_t other = watchcontrols::None,
                           uint32_t elapsedMs = 0, uint32_t sampleGapMs = 0,
                           uint32_t readUs = 0, uint8_t flags = 0) {
    if (!_touchTraceEnabled) return;
    uint16_t index = (_touchTraceHead + _touchTraceCount) % kTouchTraceCapacity;
    if (_touchTraceCount == kTouchTraceCapacity) {
        index = _touchTraceHead;
        _touchTraceHead = (_touchTraceHead + 1) % kTouchTraceCapacity;
        ++_touchTraceDropped;
    } else {
        ++_touchTraceCount;
    }
    _touchTrace[index] = {atMs, x, y, target, other, traceClamp16(elapsedMs),
                          traceClamp16(sampleGapMs), traceClamp16(readUs),
                          (uint8_t)kind, (uint8_t)_screen, flags};
}

static void setTouchTraceEnabled(bool enabled) {
    if(_touchIrqAttached) {
        detachInterrupt(digitalPinToInterrupt(13));
        _touchIrqAttached=false;
    }
    _touchTraceEnabled = enabled;
    _touchTraceHead = _touchTraceCount = 0;
    _touchTraceDropped = 0;
    _traceLastHeldMs = _traceLastDelayMs = 0;
    _traceAcquireCount = 0;
    _traceMaxSampleGapMs = _traceMaxReadUs = 0;
    _traceRawKnown = false;
    _traceRawContact = false;
    _touchIrqHead = _touchIrqCount = 0;
    _touchIrqTotal = _touchIrqDropped = 0;
    if(enabled) {
        attachInterrupt(digitalPinToInterrupt(13), touchTraceIrq, FALLING);
        _touchIrqAttached=true;
    }
}

static void stopTouchTrace() {
    _touchTraceEnabled = false;
    if(_touchIrqAttached) {
        detachInterrupt(digitalPinToInterrupt(13));
        _touchIrqAttached=false;
    }
}

static const char* touchTraceKindName(TouchTraceKind kind) {
    switch (kind) {
        case TouchTraceKind::Raw: return "raw";
        case TouchTraceKind::Down: return "down";
        case TouchTraceKind::Scroll: return "scroll";
        case TouchTraceKind::End: return "end";
        case TouchTraceKind::Screen: return "screen";
    }
    return "?";
}

static const char* touchEndReasonName(TouchEndReason reason) {
    switch (reason) {
        case TouchEndReason::Accepted: return "accepted";
        case TouchEndReason::NoTarget: return "no_target";
        case TouchEndReason::Scrolled: return "scrolled";
        case TouchEndReason::TargetChanged: return "target_changed";
        case TouchEndReason::Timeout: return "timeout";
        case TouchEndReason::Outside: return "outside";
    }
    return "?";
}

static void dumpTouchTrace() {
    bool wasEnabled = _touchTraceEnabled;
    stopTouchTrace();
    Serial.printf("TRACE begin count=%u dropped=%lu enabled=%u acquisitions=%lu max_gap=%u max_read_us=%u irq=%lu irq_kept=%u irq_dropped=%lu\n",
                  _touchTraceCount, (unsigned long)_touchTraceDropped, wasEnabled,
                  (unsigned long)_traceAcquireCount, _traceMaxSampleGapMs, _traceMaxReadUs,
                  (unsigned long)_touchIrqTotal, _touchIrqCount, (unsigned long)_touchIrqDropped);
    for (uint8_t i = 0; i < _touchIrqCount; ++i) {
        Serial.printf("TRACEIRQ t_us=%lu\n",
                      (unsigned long)_touchIrqTimes[(_touchIrqHead + i) % kTouchIrqCapacity]);
    }
    for (uint16_t i = 0; i < _touchTraceCount; ++i) {
        const auto& e = _touchTrace[(_touchTraceHead + i) % kTouchTraceCapacity];
        auto kind = (TouchTraceKind)e.kind;
        if (kind == TouchTraceKind::Raw) {
            Serial.printf("TRACE t=%lu kind=%s screen=%u contact=%u int_low=%u x=%d y=%d gap=%u read_us=%u\n",
                          (unsigned long)e.atMs, touchTraceKindName(kind), e.screen,
                          (e.flags & 1) != 0, (e.flags & 2) != 0, e.x, e.y,
                          e.sampleGapMs, e.readUs);
        } else if (kind == TouchTraceKind::End) {
            Serial.printf("TRACE t=%lu kind=%s screen=%u x=%d y=%d captured=%d released=%d elapsed=%u reason=%s\n",
                          (unsigned long)e.atMs, touchTraceKindName(kind), e.screen, e.x, e.y,
                          e.target, e.other, e.elapsedMs, touchEndReasonName((TouchEndReason)e.flags));
        } else if (kind == TouchTraceKind::Screen) {
            Serial.printf("TRACE t=%lu kind=%s from=%d to=%d cause=%d\n", (unsigned long)e.atMs,
                          touchTraceKindName(kind), e.target, e.other, e.flags);
        } else {
            Serial.printf("TRACE t=%lu kind=%s screen=%u x=%d y=%d target=%d elapsed=%u\n",
                          (unsigned long)e.atMs, touchTraceKindName(kind), e.screen, e.x, e.y,
                          e.target, e.elapsedMs);
        }
    }
    Serial.println("TRACE end");
    Serial.flush();
}

static bool hit(int16_t x, int16_t y, int16_t left, int16_t top, int16_t width, int16_t height) {
    return x >= left && x < left + width && y >= top && y < top + height;
}

static bool tapHit(int16_t left, int16_t top, int16_t width, int16_t height) {
    return hit(_tapX, _tapY, left, top, width, height)
        && hit(_touch.startX(), _touch.startY(), left, top, width, height);
}

static bool actualBotContains(int16_t x,int16_t y) {
    const auto& metrics=face.bot().metrics();
    if(metrics.bodyR<=0) return false;
    int32_t dx=x-(kBotX+metrics.cx),dy=y-(kBotY+metrics.cy);
    return dx*dx+dy*dy<=(int32_t)metrics.bodyR*metrics.bodyR;
}

static void directFaceGaze(int16_t x,int16_t y,uint32_t now) {
    const auto& metrics=face.bot().metrics();
    float gx=0.0f,gy=0.0f;
    if(!actualBotContains(x,y)) {
        float dx=(float)(x-(kBotX+metrics.cx));
        float dy=(float)(y-(kBotY+metrics.cy));
        float length=sqrtf(dx*dx+dy*dy);
        if(length>0.0f) { gx=dx/length; gy=dy/length; }
    }
    _gazeUntil=now+2200;
    face.bot().setMood(botux::BotUx::Mood::Idle);
    face.bot().setExpression(botux::BotUx::Expression::Auto,180);
    face.bot().setAnimation(botux::BotUx::Animation::Auto);
    face.bot().setTalking(false);
    _drawMood=botux::BotUx::Mood::Idle;
    face.bot().gazeAt(gx,gy,2200);
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

static void clickSound()   { _sounds.play(ux::sound::Cue::Select); }
static void confirmSound() { _sounds.play(ux::sound::Cue::Confirm); }
static void pokeSound()    { _sounds.play(ux::sound::Cue::Poke); }

static void resetUiPointer() {
    _faceTracking=false; _pointer.cancel(); _clickUntil=0; _settingsScroll.cancel(); _personalScroll.cancel(); _editorScroll.cancel();
    _capturedTarget=watchcontrols::None; _namePressedKey = -1; _colorDrag = 0;
}

static void consumeWakeInput() {
    _buttonA.consume(M5.BtnA.isPressed());
    _buttonB.consume(M5.BtnB.isPressed());
    _touch.consume();
    resetUiPointer();
}

static void applySettings() {
    watchstrings::chinese()=settings.data().language!=0;
    settings.rebuildStyle();
    power.setIndicator(settings.data().indicator);
    _sounds.setEnabled(settings.data().sound);
    settings.apply(face.bot());
    settings.apply(previewBot);
    face.bot().setName(settings.data().botName);
    previewBot.setName(settings.data().botName);
    face.bot().setGazeDirection((botux::BotUx::GazeDirection)settings.data().gaze);
    previewBot.setGazeDirection((botux::BotUx::GazeDirection)settings.data().gaze);
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
    if (_screen != Screen::Face) return botux::BotUx::Mood::Listening;
    if ((int32_t)(_gazeUntil - now) > 0) return botux::BotUx::Mood::Idle;
    if (_manualPreset) return _manualCombo.mood;
    if (_happyAcknowledgment.active(now)) return botux::BotUx::Mood::Happy;
    if (_battery <= kLowBattery) return botux::BotUx::Mood::Sleepy;
    return settings.data().expression == 0 ? _ambientMood : botux::BotUx::Mood::Idle;
}

static void showManualMood(botux::BotUx::Mood mood) {
    _gazeUntil=0; face.bot().clearGaze();
    _manualPreset=true;
    _manualCombo={mood,botux::BotUx::Expression::Auto,botux::BotUx::Animation::Auto};
    face.bot().setExpression(botux::BotUx::Expression::Auto,300);
    face.bot().setAnimation(botux::BotUx::Animation::Auto);
    face.bot().setTalking(mood==botux::BotUx::Mood::Speaking);
    _drawMood=(botux::BotUx::Mood)0xFF;
    face.invalidate(); clickSound();
}

static void enterSettings() {
    resetUiPointer();
    _ambientCycle.postpone(millis());
    Screen prior = _screen;
    _screen = Screen::Settings;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen);
    _editor = Editor::None;
    _menu = MenuItem::Time;
    _settingsList.configure(kMenuCount, kVisibleRows);
    _settingsList.select(0);
    _settingsScroll.setBounds(kMenuCount * kMenuStep, 288);
    _settingsScroll.setOffset(0);
    _buttonNavigation=false;
    _uiDirty = true;
    confirmSound();
}

static void enterPersonalize(Screen parent = Screen::Settings) {
    resetUiPointer();
    _ambientCycle.postpone(millis());
    _personalParent = parent;
    Screen prior = _screen;
    _screen = Screen::Personalize;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen);
    _editor = Editor::None;
    _personal = PersonalItem::Expression;
    _personalList.configure(kPersonalCount, kVisibleRows);
    _personalList.select(0);
    _personalScroll.setBounds(kPersonalCount * kMenuStep, 288);
    _personalScroll.setOffset(0);
    _uiDirty = true;
    confirmSound();
}

static void leavePersonalize(int cause = watchcontrols::None) {
    resetUiPointer();
    Screen prior = _screen;
    _screen = _personalParent;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen,
                   0, 0, 0, (uint8_t)cause);
    _editor = Editor::None;
    _uiDirty = true;
    if (_screen == Screen::Face) {
        _faceNeedsClear = true;
        face.invalidate();
    }
    clickSound();
}

static void leaveSettings(int cause = watchcontrols::None) {
    resetUiPointer();
    Screen prior = _screen;
    _screen = Screen::Face;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen,
                   0, 0, 0, (uint8_t)cause);
    _editor = Editor::None;
    settings.save();
    _faceNeedsClear = true;
    face.invalidate();
    confirmSound();
}

static void enterEditor(Editor editor, Screen parent = Screen::Settings) {
    resetUiPointer();
    Screen prior = _screen;
    _screen = Screen::Editor;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen);
    _editor = editor;
    _originalSettings = settings.data();
    _originalManualPreset = _manualPreset; _originalManualCombo = _manualCombo;
    _editorParent = parent;
    _editField = 0;
    if(watchcontrols::editorUsesPreviewList(editor)) {
        auto layout=watchcontrols::previewList();
        _editorScroll.setBounds(watchcontrols::editorRowCount(editor)*layout.step,layout.h);
        _editorScroll.setOffset(0);
    } else _editorScroll.setBounds(0,0);
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
    resetUiPointer();
    _manualPreset = _originalManualPreset; _manualCombo = _originalManualCombo;
    settings.data() = _originalSettings;
    applySettings();
    Screen prior = _screen;
    _screen = _editorParent;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen);
    _editor = Editor::None;
    _uiDirty = true;
    clickSound();
}

static void returnHome() {
    if(_screen==Screen::Editor) {
        _manualPreset=_originalManualPreset; _manualCombo=_originalManualCombo;
        settings.data()=_originalSettings; applySettings();
    }
    _dozing=false; _clockDoubleTap.reset(); consumeWakeInput();
    Screen prior=_screen;
    _screen=Screen::Face; _editor=Editor::None;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen);
    _faceNeedsClear=true; face.invalidate(); _uiDirty=true;
    power.applyLevel(settings.data().brightness);
    _sounds.play(ux::sound::Cue::Back);
}

static void saveEditor(int cause = watchcontrols::None) {
    resetUiPointer();
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
    Screen prior = _screen;
    _screen = _editorParent;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen,
                   0, 0, 0, (uint8_t)cause);
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
    face.bot().setTalking(false);
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
    } else if (_editor == Editor::Gaze) {
        settings.data().gaze = (settings.data().gaze + botux::BotUx::gazeDirectionCount() + delta) % botux::BotUx::gazeDirectionCount();
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
        } else if(_editField==2) {
            settings.data().sound = !settings.data().sound;
        } else { settings.data().indicator=!settings.data().indicator; }
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
        case PersonalItem::Gaze: enterEditor(Editor::Gaze, Screen::Personalize); break;
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
            _clockDoubleTap.reset();
            return;
        }
        const int clockY=settings.data().swapLayout?20:376;
        if(_clockDoubleTap.tap(tapHit(90,clockY,286,70),millis())) { enterSettings(); return; }
        directFaceGaze(_tapX,_tapY,millis());
    } else if (gesture == Gesture::Long) {
        _clockDoubleTap.reset();
        if (actualBotContains(_touch.startX(),_touch.startY())&&actualBotContains(_tapX,_tapY))
            enterPersonalize(Screen::Face);
    } else if (gesture != Gesture::None) {
        _clockDoubleTap.reset();
        if (gesture == Gesture::SwipeDown && _touch.startY() <= 20)
            showStatusPanel(millis());
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

static void activateUiControl(int target) {
    using namespace watchcontrols;
    if(_screen==Screen::Settings || _screen==Screen::Personalize) {
        if(target==Done) { if(_screen==Screen::Personalize) leavePersonalize(target); else leaveSettings(target); return; }
    }
    if (_screen == Screen::Settings && target >= MenuRow) {
        _settingsList.select(target-MenuRow); _menu=(MenuItem)_settingsList.selected(); selectMenuItem(); return;
    }
    if (_screen == Screen::Personalize && target >= PersonalRow) {
        _personalList.select(target-PersonalRow); _personal=(PersonalItem)_personalList.selected(); selectPersonalItem(); return;
    }
    if (_screen != Screen::Editor) return;
    if (target == Done) { saveEditor(target); return; }
    int field=target-First;
    if (_editor == Editor::Name && target >= NameKey && target < NameKey+30) {
        bool done=_nameEditor.press(target-NameKey); clickSound(); _uiDirty=true;
        if(done) saveEditor(target);
    } else if (_editor == Editor::Language) {
        settings.data().language=field; applySettings(); clickSound();
    } else if (_editor == Editor::Layout || _editor == Editor::Preview || _editor == Editor::Gaze
            || _editor == Editor::Appearance || _editor == Editor::Motion || _editor == Editor::Display) {
        _editField=field; changeEditorValue(_tapX<233?-1:1);
    } else if (_editor == Editor::Color && target==First) {
        settings.data().customColor=false; applySettings(); clickSound();
    } else if (_editor == Editor::Time || _editor == Editor::Date) {
        _editField=field/3;
        if(field%3==2) { _uiDirty=true; clickSound(); }
        else changeEditorValue(field%3==0?-1:1);
    } else if (_editor == Editor::Format) {
        if(field<2) settings.data().hour24=field==1;
        else settings.data().showSeconds=!settings.data().showSeconds;
        applySettings(); clickSound();
    } else if (_editor == Editor::Expression) {
        if(field<2) cycleExpression(field==0?-1:1);
        else { previewBot.poke(); pokeSound(); }
    }
}

static int exactPressedTarget() {
    if(!_pointer.pressed()) return watchcontrols::None;
    float offset=_screen==Screen::Settings?_settingsScroll.offset():
        _screen==Screen::Personalize?_personalScroll.offset():
        _screen==Screen::Editor&&watchcontrols::editorUsesPreviewList(_editor)?_editorScroll.offset():0;
    auto target=watchcontrols::at(_screen,_editor,offset,_pointer.x(),_pointer.y());
    return target.id==_capturedTarget?target.id:watchcontrols::None;
}

// Hardware and diagnostic input share this path; one session owns each gesture.
static void handleUiPointer(bool down, bool held, bool up, int x, int y, uint32_t now) {
    if (_screen==Screen::Face) return;
    ux::ScrollModel* scroll=_screen==Screen::Settings?&_settingsScroll:
        _screen==Screen::Personalize?&_personalScroll:
        _screen==Screen::Editor&&watchcontrols::editorUsesPreviewList(_editor)?&_editorScroll:nullptr;
    const auto scrollLayout=_screen==Screen::Editor?watchcontrols::previewList():watchcontrols::mainList();
    int oldPressed=exactPressedTarget();
    if(down) {
        _buttonNavigation=false; _uiDirty=true; _listBandOnly=false;
        if(scroll) scroll->cancel(); // Stop inertia before capturing the visible row.
        auto target=watchcontrols::at(_screen,_editor,scroll?scroll->offset():0,x,y);
        _capturedTarget=target.id;
        _pointer.begin(target.id,target.bounds,x,y,now,scroll&&y>=scrollLayout.y&&y<scrollLayout.y+scrollLayout.h);
        pushTouchTrace(TouchTraceKind::Down, now, x, y, target.id);
        _colorDrag=target.id==watchcontrols::ColorPad?1:target.id==watchcontrols::HueBar?2:0;
    }
    if(_pointer.active() && (held||up)) {
        bool wasScrolling=_pointer.scrolling();
        _pointer.move(x,y);
        if(scroll && _pointer.scrolling()) {
            if(!wasScrolling) {
                scroll->begin(_pointer.startY(),_pointer.startedAt());
                pushTouchTrace(TouchTraceKind::Scroll, now, x, y, _capturedTarget,
                               watchcontrols::None, now-_pointer.startedAt());
            }
            float before=scroll->offset(); scroll->move(y,now);
            if(before!=scroll->offset()) markListMoved();
        }
        if(_colorDrag) updateColorPicker(x,y);
    }
    if(up && _pointer.active()) {
        bool dragged=_pointer.scrolling();
        auto releasedBounds=_pointer.bounds();
        auto releasedTarget=watchcontrols::at(_screen,_editor,scroll?scroll->offset():0,x,y);
        uint32_t elapsed=now-_pointer.startedAt();
        int captured=_capturedTarget;
        TouchEndReason reason=TouchEndReason::Accepted;
        if(captured==watchcontrols::None) reason=TouchEndReason::NoTarget;
        else if(dragged) reason=TouchEndReason::Scrolled;
        else if(releasedTarget.id!=captured) reason=TouchEndReason::TargetChanged;
        else if(elapsed>1000) reason=TouchEndReason::Timeout;
        if(!dragged&&releasedTarget.id!=_capturedTarget) _pointer.cancel();
        int clicked=_pointer.end(x,y,now);
        if(clicked<0&&reason==TouchEndReason::Accepted) reason=TouchEndReason::Outside;
        pushTouchTrace(TouchTraceKind::End, now, x, y, captured, releasedTarget.id,
                       elapsed, 0, 0, (uint8_t)reason);
        if(clicked>=0) { _clickRect=releasedBounds; _clickUntil=now+90; }
        if(scroll && dragged) scroll->end(now);
        _colorDrag=0;
        _capturedTarget=watchcontrols::None;
        _tapX=x; _tapY=y;
        if(clicked>=0 && clicked<watchcontrols::ColorPad) activateUiControl(clicked);
    }
    int pressed=exactPressedTarget();
    _namePressedKey=pressed>=watchcontrols::NameKey&&pressed<watchcontrols::NameKey+30?pressed-watchcontrols::NameKey:-1;
    if(pressed!=oldPressed) { _uiDirty=true; _listBandOnly=false; }
}

static void handleInputs(uint32_t now) {
    bool contact=_contact.isPressed(); int32_t x=_contact.x,y=_contact.y;
    if(_diagnosticContact) { contact=_diagnosticDown; x=_diagnosticX; y=_diagnosticY; }
    else if(now-_contactReadMs>=8) {
        uint32_t previousReadMs=_contactReadMs;
        _contactReadMs=now;
        uint32_t readStartUs=micros();
        contact=M5.Display.getTouch(&x,&y);
        uint32_t readUs=micros()-readStartUs;
        uint32_t sampleGapMs=previousReadMs?now-previousReadMs:0;
        if(_touchTraceEnabled) {
            ++_traceAcquireCount;
            if(sampleGapMs>_traceMaxSampleGapMs) _traceMaxSampleGapMs=traceClamp16(sampleGapMs);
            if(readUs>_traceMaxReadUs) _traceMaxReadUs=traceClamp16(readUs);
            bool changed=!_traceRawKnown||contact!=_traceRawContact;
            bool heldCheckpoint=contact&&(now-_traceLastHeldMs>=100);
            // M5GFX's StopWatch board setup maps CST820 INT to GPIO13. Low means
            // the controller signalled an event; it does not identify an I2C error.
            bool intLow=digitalRead(13)==LOW;
            bool delayed=sampleGapMs>=24&&(contact||intLow||now-_traceLastDelayMs>=1000);
            if(changed||heldCheckpoint||delayed) {
                uint8_t flags=(contact?1:0)|(intLow?2:0);
                pushTouchTrace(TouchTraceKind::Raw,now,x,y,watchcontrols::None,
                               watchcontrols::None,0,sampleGapMs,readUs,flags);
                if(contact) _traceLastHeldMs=now;
                if(sampleGapMs>=24) _traceLastDelayMs=now;
            }
            _traceRawKnown=true;
            _traceRawContact=contact;
        }
    }
    _contact.sample(contact,x,y);
    const auto& touch=_contact;
    if(M5.BtnPWR.wasClicked()) {
        returnHome(); return;
    }
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
    Gesture gt = Gesture::None;
    if (_screen == Screen::Face) {
        if(touch.wasPressed()) _faceTracking=true;
        if(touch.isPressed()&&_faceTracking&&now-_gazeTouchMs>=33) {
            _gazeTouchMs=now; directFaceGaze(touch.x,touch.y,now);
        }
        if(touch.wasReleased()) _faceTracking=false;
        gt = _touch.poll(touch.wasPressed(),touch.isPressed(),touch.wasReleased(),touch.x,touch.y,now,3000);
        if(gt==Gesture::Tap) { _tapX=_touch.tapX(); _tapY=_touch.tapY(); }
        else if(gt!=Gesture::None) { _tapX=touch.x; _tapY=touch.y; }
    } else handleUiPointer(touch.wasPressed(),touch.isPressed(),touch.wasReleased(),touch.x,touch.y,now);

    if (_screen == Screen::Face) {
        if (ga == Gesture::Tap) {
            _bClick.cancel();
            uint8_t current=(uint8_t)(_manualPreset?_manualCombo.mood:face.bot().mood());
            auto mood=(botux::BotUx::Mood)_moodDeck.random(current);
            showManualMood(mood);
        }
        if (gb == Gesture::Long) { _bClick.cancel(); startDoze(); }
        auto click=_bClick.poll(gb==Gesture::Tap,now);
        if(click==watchinteraction::SingleDoubleClick::Event::Single) {
            uint8_t current=(uint8_t)(_manualPreset?_manualCombo.mood:face.bot().mood());
            auto mood=(botux::BotUx::Mood)_moodDeck.next(current);
            showManualMood(mood);
        } else if(click==watchinteraction::SingleDoubleClick::Event::Double) {
            _manualPreset=false;
            settings.data().expression=settings.data().animation=0;
            face.bot().resetToIdle(); face.bot().setTalking(false);
            _ambientMood=_drawMood=botux::BotUx::Mood::Idle;
            _ambientCycle.postpone(now);
            face.invalidate(); confirmSound();
        }
        if (gt != Gesture::None) handleFaceInput(gt);
        if (ga != Gesture::None || gb != Gesture::None || gt != Gesture::None) _ambientCycle.postpone(now);
        return;
    }
    _bClick.cancel();

    if (_screen == Screen::Settings) {
        if (ga == Gesture::Tap) { _buttonNavigation=true; selectMenuItem(); }
        else if (ga == Gesture::Long) leaveSettings();
        if (gb == Gesture::Tap) {
            _buttonNavigation=true;
            _settingsList.move(1);
            _menu = (MenuItem)_settingsList.selected();
            revealRow(_settingsScroll, (uint8_t)_menu);
            _uiDirty = true;
            clickSound();
        }
        return;
    }

    if (_screen == Screen::Personalize) {
        if (ga == Gesture::Tap) { _buttonNavigation=true; selectPersonalItem(); }
        else if (ga == Gesture::Long) leavePersonalize();
        if (gb == Gesture::Tap) {
            _buttonNavigation=true;
            _personalList.move(1);
            _personal = (PersonalItem)_personalList.selected();
            revealRow(_personalScroll, (uint8_t)_personal);
            _uiDirty = true;
        }
        return;
    }

    if (ga == Gesture::Tap) { _buttonNavigation=true; changeEditorValue(-1); }
    else if (ga == Gesture::Long) cancelEditor();
    if (gb == Gesture::Tap) { _buttonNavigation=true; changeEditorValue(1); }
    else if (gb == Gesture::Long) saveEditor();

}

static bool pointerFeedback(ux::Rect& r) {
    if(exactPressedTarget()!=watchcontrols::None) { r=_pointer.bounds(); return true; }
    if(_clickUntil && (int32_t)(millis()-_clickUntil)<0) { r=_clickRect; return true; }
    return false;
}
static bool pressedOver(int x,int y,int w,int h) {
    ux::Rect r; if(!pointerFeedback(r)) return false;
    return hit(r.x+r.w/2,r.y+r.h/2,x,y,w,h);
}
static void drawPointerFeedback() {
    // Each control paints feedback in its own exact hit geometry.
}

static void drawPill(int16_t x, int16_t y, int16_t w, int16_t h,
                     const char* label, bool selected, bool accentFill = false) {
    uint16_t fill = accentFill ? settings.style().accentColor : settings.panel();
    if(pressedOver(x,y,w,h)) fill=ux::blend565(fill,settings.ink(),48);
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

static void drawFooter();

static void drawSettingsList(bool chrome = true) {
    if (chrome) drawTitle("SETTINGS");
    const auto layout=watchcontrols::mainList();
    canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    for (uint8_t i = 0; i < kMenuCount; ++i) {
        auto row=watchcontrols::rowBounds(layout,i,_settingsScroll.offset());
        int16_t cy=row.y+row.h/2;
        if(row.y+row.h<=layout.y||row.y>=layout.y+layout.h) continue;
        bool selected=_buttonNavigation&&i==(uint8_t)_menu;
        bool pressed=pressedOver(row.x,row.y,row.w,row.h);
        ux::roundRect(canvas,row.x,row.y,row.w,row.h,layout.radius,pressed?ux::blend565(settings.panel(),settings.style().accentColor,55):selected?settings.panel():settings.style().bgColor);
        if(selected) ux::strokeRoundRect(canvas,row.x,row.y,row.w,row.h,layout.radius,settings.style().accentColor);

        canvas.setTextDatum(middle_left);
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextSize(1.0f);
        canvas.setTextColor(selected ? settings.style().accentColor : settings.ink());
        watchText(canvas,kMenuLabels[i],84,cy);

        char value[40] = {};
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
            watchText(canvas,value,382,cy);
        }
    }
    canvas.clearClipRect();
    if (chrome) drawFooter();
}

static void drawPersonalizeList(bool chrome = true) {
    if (chrome) drawTitle("BOT PERSONALITY");
    const auto layout=watchcontrols::mainList();
    canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    for (uint8_t i = 0; i < kPersonalCount; ++i) {
        auto row=watchcontrols::rowBounds(layout,i,_personalScroll.offset());
        int16_t cy=row.y+row.h/2;
        if(row.y+row.h<=layout.y||row.y>=layout.y+layout.h) continue;
        bool selected=_buttonNavigation&&i==(uint8_t)_personal;
        bool pressed=pressedOver(row.x,row.y,row.w,row.h);
        ux::roundRect(canvas,row.x,row.y,row.w,row.h,layout.radius,pressed?ux::blend565(settings.panel(),settings.style().accentColor,55):selected?settings.panel():settings.style().bgColor);
        if(selected) ux::strokeRoundRect(canvas,row.x,row.y,row.w,row.h,layout.radius,settings.style().accentColor);
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(selected ? settings.style().accentColor : settings.ink());
        watchText(canvas,kPersonalLabels[i],84,cy);
        char value[40] = {};
        switch ((PersonalItem)i) {
            case PersonalItem::Expression: snprintf(value, sizeof(value), "%s", Settings::expressionName(settings.data().expression)); break;
            case PersonalItem::Action: snprintf(value, sizeof(value), "%s", Settings::animationName(settings.data().animation)); break;
            case PersonalItem::Appearance: snprintf(value, sizeof(value), "%s", Settings::appearanceName(settings.data().appearance)); break;
            case PersonalItem::Color: snprintf(value, sizeof(value), settings.data().customColor ? "CUSTOM" : "THEME"); break;
            case PersonalItem::Name: snprintf(value, sizeof(value), "%s", settings.data().botName); break;
            case PersonalItem::Language: snprintf(value, sizeof(value), settings.data().language ? "中文" : "English"); break;
            case PersonalItem::Preview: snprintf(value, sizeof(value), "960"); break;
            case PersonalItem::Gaze: snprintf(value,sizeof(value),"%s",botux::BotUx::gazeDirectionName((botux::BotUx::GazeDirection)settings.data().gaze,settings.data().language?botux::BotUx::Language::Chinese:botux::BotUx::Language::English)); break;
            case PersonalItem::Intensity: snprintf(value, sizeof(value), "%u / 5", settings.data().motionAmount); break;
            case PersonalItem::Speed: snprintf(value, sizeof(value), "%u / 5", settings.data().animationSpeed); break;
            default: break;
        }
        if (value[0]) {
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextDatum(middle_right);
            canvas.setTextColor(settings.muted());
            if(i==(uint8_t)PersonalItem::Name) watchEllipsizedText(canvas,value,382,cy,214);
            else watchText(canvas,value,382,cy);
        }
    }
    canvas.clearClipRect();
    if (chrome) drawFooter();
}

static void drawFooter() {
    auto bounds=watchcontrols::doneBounds();
    drawPill(bounds.x,bounds.y,bounds.w,bounds.h,"DONE",false,true);
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
    drawStepper(157,"HOUR",hour,_buttonNavigation&&_editField==0,100);
    drawStepper(309,"MINUTE",minute,_buttonNavigation&&_editField==1,100);
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
    drawStepper(108,"MONTH",kMonths[_editMonth-1],_buttonNavigation&&_editField==0,86);
    drawStepper(233,"DAY",day,_buttonNavigation&&_editField==1,86);
    drawStepper(358,"YEAR",year,_buttonNavigation&&_editField==2,86);
    drawFooter();
}

static void drawFormatEditor() {
    drawTitle("TIME FORMAT");
    drawPill(78, 128, 146, 76, "12 HOUR", !settings.data().hour24, !settings.data().hour24);
    drawPill(242, 128, 146, 76, "24 HOUR", settings.data().hour24, settings.data().hour24);
    drawPill(100, 242, 266, 58, settings.data().showSeconds ? "SECONDS  ON" : "SECONDS  OFF",
             _buttonNavigation&&_editField==1, settings.data().showSeconds);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted());
    watchText(canvas, "Show seconds on the clock", 233, 330);
    drawFooter();
}

static void drawArrowRow(int16_t cy, const char* label, const char* value, bool selected) {
    if (selected || pressedOver(58,cy-22,350,44)) ux::roundRect(canvas,58,cy-22,350,44,17,
        pressedOver(58,cy-22,350,44)?ux::blend565(settings.panel(),settings.style().accentColor,55):settings.panel());
    canvas.setTextDatum(middle_left);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted()); watchText(canvas,label,84,cy);
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(settings.ink()); watchText(canvas,value,382,cy);
    canvas.setTextColor(settings.style().accentColor);
    canvas.setTextDatum(middle_center);
    watchText(canvas,"<",68,cy); watchText(canvas,">",398,cy);
}

static void drawPreviewArrowRow(uint8_t index,const char* label,const char* value) {
    const auto layout=watchcontrols::previewList();
    auto row=watchcontrols::rowBounds(layout,index,_editorScroll.offset());
    if(row.y+row.h<=layout.y||row.y>=layout.y+layout.h) return;
    drawArrowRow(row.y+row.h/2,label,value,_buttonNavigation&&_editField==index);
}

static void drawAppearanceEditor() {
    drawTitle("APPEARANCE");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, kPreviewY);
    auto layout=watchcontrols::previewList(); canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    drawPreviewArrowRow(0,"SHAPE",Settings::appearanceName(settings.data().appearance));
    drawPreviewArrowRow(1,"EYES",Settings::eyeStyleName(settings.data().eyeStyle));
    canvas.clearClipRect();
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
    watchText(canvas, "Tap arrows; tap the bot to react", 233, 326);
    drawFooter();
}

static void drawMotionEditor() {
    drawTitle("MOTION");
    previewBot.draw();
    previewSprite.pushRotateZoomWithAA(&canvas,233,129,0,0.72f,0.72f);
    char amount[8], speed[8];
    snprintf(amount, sizeof(amount), "%u / 5", settings.data().motionAmount);
    snprintf(speed, sizeof(speed), "%u / 5", settings.data().animationSpeed);
    auto layout=watchcontrols::previewList(); canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    drawPreviewArrowRow(0,"ACTION",Settings::animationName(settings.data().animation));
    drawPreviewArrowRow(1,"WRIST",settings.data().motion?"ON":"OFF");
    drawPreviewArrowRow(2,"INTENSITY",amount);
    drawPreviewArrowRow(3,"SPEED",speed);
    canvas.clearClipRect();
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
    drawArrowRow(170,"BRIGHTNESS",brightness,_buttonNavigation&&_editField==0);
    drawArrowRow(230,"THEME",Settings::themeName(settings.data().theme),_buttonNavigation&&_editField==1);
    drawArrowRow(290,"SOUND",settings.data().sound?"ON":"OFF",_buttonNavigation&&_editField==2);
    drawArrowRow(350,"INDICATOR",settings.data().indicator?"ON":"OFF",_buttonNavigation&&_editField==3);
    drawFooter();
}

static void drawNameEditor() {
    drawTitle("BOT NAME");
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextColor(settings.ink());
    watchEllipsizedText(canvas, _nameEditor.text(), 233, 128, 310);
    static const ux::NameKeyboardLabels zhKeys={"删除","空格","Aa","确定"};
    ux::drawNameKeyboard(canvas, _nameEditor, ux::Rect{78,170,310,205},
                         settings.panel(), settings.ink(), watchKeyboardFont(settings.data().language),
                         _namePressedKey,settings.data().language?&zhKeys:nullptr);
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
    drawArrowRow(170,"BOT TEXT",settings.data().showDescription?"SHOW":"HIDE",_buttonNavigation&&_editField==0);
    drawArrowRow(250,"TOP",settings.data().swapLayout?"TIME":"BOT TEXT",_buttonNavigation&&_editField==1);
    drawFooter();
}

static void drawCombinationEditor() {
    drawTitle("COMBINATIONS");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, 62);
    auto layout=watchcontrols::previewList(); canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    drawPreviewArrowRow(0,"STATE",botux::BotUx::moodName((botux::BotUx::Mood)_previewMood));
    drawPreviewArrowRow(1,"FACE",botux::BotUx::expressionName((botux::BotUx::Expression)_previewExpression));
    drawPreviewArrowRow(2,"ACTION",botux::BotUx::animationName((botux::BotUx::Animation)_previewAnimation));
    canvas.clearClipRect();
    drawFooter();
}

static void drawGazeEditor() {
    drawTitle("GAZE"); previewBot.draw();
    previewSprite.pushSprite(&canvas,kPreviewX,62);
    auto layout=watchcontrols::previewList(); canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    drawPreviewArrowRow(0,"DIRECTION",botux::BotUx::gazeDirectionName((botux::BotUx::GazeDirection)settings.data().gaze,settings.data().language?botux::BotUx::Language::Chinese:botux::BotUx::Language::English));
    canvas.clearClipRect();
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
        case Editor::Gaze: drawGazeEditor(); break;
        default: break;
    }
}

static bool previewIsAnimated() {
    return _screen == Screen::Editor
        && (_editor == Editor::Gaze || _editor == Editor::Preview || _editor == Editor::Expression || _editor == Editor::Appearance
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

    Serial.printf("PERF screen=%u fps=%.1f update=%luus draw=%luus push=%luus max=%luus heap=%u largest=%u psram=%u sound=%u sound_fail=%lu\n",
                  (unsigned)_screen, (double)_telemetry.frames * 1000.0 / elapsed,
                  (unsigned long)(_telemetry.updateUs / _telemetry.frames),
                  (unsigned long)(_telemetry.drawUs / _telemetry.frames),
                  (unsigned long)(_telemetry.pushUs / _telemetry.frames),
                  (unsigned long)_telemetry.maxFrameUs,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)ESP.getFreePsram(),_soundReady,(unsigned long)_soundOutput.failures());
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
    bool gazeActive=_screen==Screen::Face&&(int32_t)(_gazeUntil-now)>0;
    auto presentation=watchcompanion::presentation(gazeActive,_manualPreset,(uint8_t)_manualCombo.mood,
        (uint8_t)_manualCombo.expression,(uint8_t)_manualCombo.animation,
        (uint8_t)selectedExpression,settings.data().animation,_dozing||_battery<=kLowBattery);
    face.bot().setExpression((botux::BotUx::Expression)presentation.expression);
    face.bot().setAnimation((botux::BotUx::Animation)presentation.animation);
    face.bot().setTalking(presentation.talking);
    face.update(now);
    // The HUD keeps the real percentage. Explicit previews temporarily suppress
    // BotUx's <=10% sleepy override so every selected mood remains visible.
    face.bot().setBattery(!_dozing&&_screen==Screen::Face&&(_manualPreset||gazeActive)?100:_battery);
    face.bot().setBatteryVisible(false);
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
    drawPointerFeedback();
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
        drawPointerFeedback();
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
    _diagnosticContact=false; _diagnosticDown=false; _contact.sample(false,0,0);
    resetUiPointer();
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
        _settingsList.configure(kMenuCount, kVisibleRows);
        _settingsList.select(page == 8 ? kMenuCount-1 : 0);
        _menu = (MenuItem)_settingsList.selected();
        _settingsScroll.setBounds(kMenuCount*kMenuStep,288);
        _settingsScroll.setOffset(page == 8 ? 9999 : page == 18 ? 37 : 0);
    } else if (page == 2 || page == 9) {
        _screen = Screen::Personalize;
        _personalList.configure(kPersonalCount, kVisibleRows);
        _personalList.select(page == 9 ? kPersonalCount-1 : 0);
        _personal = (PersonalItem)_personalList.selected();
        _personalScroll.setBounds(kPersonalCount*kMenuStep,288);
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
                : (page == 13 || page == 20 || page == 22) ? Editor::Preview
                : page == 14 ? Editor::Layout
                : page == 15 ? Editor::Language
                : page == 21 ? Editor::Gaze : Editor::Time;
        if (page == 12) _nameEditor.begin(settings.data().botName);
        if (page == 22) { _previewMood=(uint8_t)botux::BotUx::Mood::Thinking; _previewExpression=_previewAnimation=0; }
        if (page == 20) {
            _previewMood = (uint8_t)botux::BotUx::Mood::Happy;
            _previewExpression = (uint8_t)botux::BotUx::Expression::Joy;
            _previewAnimation = (uint8_t)botux::BotUx::Animation::Wave;
        }
        _editField = 0;
        if(watchcontrols::editorUsesPreviewList(_editor)) {
            auto layout=watchcontrols::previewList();
            _editorScroll.setBounds(watchcontrols::editorRowCount(_editor)*layout.step,layout.h);
            _editorScroll.setOffset(0);
        }
    }
    _uiDirty = true;
}

static uint32_t _diagnosticSeq=0;
static void printUiState() {
    Serial.printf("UI seq=%lu screen=%u editor=%u pressed=%d scrolling=%u offset=%.1f sound=%u language=%u gaze=%u indicator=%u status=%u manual=%u mood=%u effective=%u expression=%u effective_expr=%u animation=%u\n",
        (unsigned long)_diagnosticSeq,(unsigned)_screen,(unsigned)_editor,exactPressedTarget(),_pointer.scrolling(),
        _screen==Screen::Personalize?_personalScroll.offset():_screen==Screen::Editor?_editorScroll.offset():_settingsScroll.offset(),settings.data().sound,settings.data().language,settings.data().gaze,settings.data().indicator,_statusPanelUntilMs!=0,
        _manualPreset,(unsigned)face.bot().mood(),(unsigned)face.bot().effectiveMood(),(unsigned)face.bot().expression(),(unsigned)face.bot().effectiveExpression(),(unsigned)face.bot().animation());
    // Drain this diagnostic reply now, rather than waiting for later telemetry.
    Serial.flush();
}
static void handleSerialCommands() {
    static char line[48]; static uint8_t length=0;
    while(Serial.available()) {
        char c=(char)Serial.read();
        if(c!='\n' && c!='\r') { if(length+1<sizeof(line)) line[length++]=c; continue; }
        if(!length) continue;
        line[length]=0; length=0;
        char* seq=strchr(line,'@'); _diagnosticSeq=seq?strtoul(seq+1,nullptr,10):0;
        if(seq) { while(seq>line && seq[-1]==' ') --seq; *seq=0; }
        int x,y,down;
        if(sscanf(line,"contact %d %d %d",&down,&x,&y)==3) {
            _diagnosticContact=true; _diagnosticDown=down!=0;
            _diagnosticX=x; _diagnosticY=y; _contactReplyPending=true;
        } else if(!strcmp(line,"physical")) { _diagnosticContact=false; printUiState(); }
        else if(!strcmp(line,"trace on")) { setTouchTraceEnabled(true); Serial.println("TRACE armed"); Serial.flush(); }
        else if(!strcmp(line,"trace off")) { stopTouchTrace(); Serial.println("TRACE disarmed"); Serial.flush(); }
        else if(!strcmp(line,"trace clear")) { bool enabled=_touchTraceEnabled; setTouchTraceEnabled(enabled); Serial.println("TRACE cleared"); Serial.flush(); }
        else if(!strcmp(line,"trace dump")) dumpTouchTrace();
        else if(!strncmp(line,"locale ",7)) { settings.data().language=atoi(line+7)==1; applySettings(); printUiState(); }
        else if(!strcmp(line,"home")) { returnHome(); printUiState(); }
        else if(!strcmp(line,"power")) {
            uint8_t cfg=0,key=0,off=0; auto& pm=M5.Power.M5pm1;
            bool ok=pm.readRegister(0x06,&cfg,1) && pm.readRegister(0x49,&key,1) && pm.readRegister(0x4a,&off,1);
            Serial.printf("POWER read=%u led_ready=%u led=%u home_ready=%u key_cfg=%02x boot_key=%02x off_cfg=%02x boot_off=%02x\n",ok,power.indicatorReady(),(cfg&16)!=0,power.homeKeyReady(),key,power.bootKeyConfig(),off,power.bootOffConfig()); Serial.flush();
        } else if((line[0]=='t') && (line[1]=='d'||line[1]=='m'||line[1]=='u') && sscanf(line+2,"%d %d",&x,&y)==2) {
            if(x>=0 && x<kW && y>=0 && y<kH) handleUiPointer(line[1]=='d',line[1]!='u',line[1]=='u',x,y,millis());
            printUiState();
        } else if(!strncmp(line,"ui",2)) printUiState();
        else if(!strncmp(line,"mood ",5)) {
            int mood=atoi(line+5);
            if(mood>=0&&mood<botux::BotUx::moodCount()) showManualMood((botux::BotUx::Mood)mood);
            printUiState();
        }
        else if(line[0]=='v' || (line[0]>='0'&&line[0]<='9')) {
            selectDiagnosticPage((uint8_t)atoi(line+(line[0]=='v'))); printUiState();
        } else if(!strcmp(line,"e")) cycleExpression(1,true);
        else if(!strcmp(line,"a")) { settings.data().animation=(settings.data().animation+1)%Settings::ANIMATION_COUNT; applySettings(); settings.save(); }
        else if(!strcmp(line,"m")) { settings.data().motion=!settings.data().motion; applySettings(); settings.save(); }
        else if(!strcmp(line,"p")) pokeBot();
        else if(!strcmp(line,"s")) showStatusPanel(millis());
        else if(!strcmp(line,"c")) emitFrameCapture();
        else if(!strncmp(line,"sound ",6)) _sounds.play((ux::sound::Cue)atoi(line+6));
    }
}

void setup() {
    auto cfg = M5.config();
    cfg.internal_imu = true;
    cfg.pmic_button = true;
    M5.begin(cfg);
    M5.Touch.end(); // Raw contacts have one host gesture owner; SDK flick state is unused.
    _soundReady=_soundOutput.begin(M5.Speaker, _sounds, 6);
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
    _settingsList.configure(kMenuCount, kVisibleRows);
    _personalList.configure(kPersonalCount, kVisibleRows);
    _ambientCycle.begin(millis());
    showStatusPanel(millis(), 1800);
    Serial.printf("bot-ux-watch ready; IMU=%d. Commands: e/a/m/p/s, c capture, v0..v22 diagnostic pages, td/tm/tu x y, ui, trace on/off/clear/dump, sound N\n",
                  M5.Imu.isEnabled());
}

void loop() {
    M5.update();
    _soundOutput.update();
    uint32_t now = millis();
    if (!_renderReady) { delay(20); return; }

    refreshPower(now);
    if(_clickUntil && (int32_t)(now-_clickUntil)>=0) { _clickUntil=0; _uiDirty=true; _listBandOnly=false; }
    updateMotion(now);
    handleInputs(now);
    if(_contactReplyPending) { _contactReplyPending=false; printUiState(); }
    _soundOutput.update();
    if (_screen == Screen::Face && !_manualPreset && !_gestureActive
        && settings.data().expression == 0 && _ambientCycle.update(now)) {
        _ambientMood=(botux::BotUx::Mood)watchcompanion::ambientMood(_ambientCycle.index());
    }
    handleSerialCommands();
    if (_diagnosticScroll && _screen == Screen::Settings) {
        _settingsScroll.setOffset((sinf(now * 0.001f) + 1) * 0.5f * (kMenuCount*kMenuStep - 288));
        markListMoved();
    }
    uint32_t scrollDt = _scrollMs ? now - _scrollMs : 0; _scrollMs = now;
    if (_screen == Screen::Settings && _settingsScroll.update(scrollDt)) markListMoved();
    if (_screen == Screen::Personalize && _personalScroll.update(scrollDt)) markListMoved();
    if (_screen == Screen::Editor && watchcontrols::editorUsesPreviewList(_editor)
        && _editorScroll.update(scrollDt)) markListMoved();

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
