// bot-ux-watch — M5StickC Plus2 watch + stopwatch hosting the shared bot-ux bot.
//
// main.cpp owns the mode state machine (Face / Stopwatch / Settings) and the
// button gesture disambiguation. Modules are plain classes with one instance
// each. No heap allocation in the loop; fixed buffers + C strings only.
#include <M5Unified.h>
#include <BotUx.h>

#include "WatchFace.h"
#include "Stopwatch.h"
#include "Settings.h"
#include "Power.h"

#include <stdio.h>
#include <string.h>

// ---- mode + gesture model -------------------------------------------------

enum class Mode : uint8_t { Face, Stopwatch, Settings };

// Settings sub-screens (value editors).
enum class SetView : uint8_t { List, TimeHour, TimeMinute, Format, Theme, Style, Brightness };

enum class MenuItem : uint8_t { TimeSet = 0, Format, Theme, Style, Brightness, Back, Count };

// Gesture: TAP (<250 ms), DOUBLE (2 taps within 300 ms), LONG (>=600 ms).
// A single tap waits a 300 ms disambiguation window before firing so a double
// can be recognized first; a long press cancels any pending tap.
enum class Gesture : uint8_t { None, Tap, Double, Long };

namespace {
constexpr uint32_t LONG_MS     = 600;
constexpr uint32_t TAP_MS      = 250;
constexpr uint32_t DOUBLE_MS   = 300;
constexpr uint32_t TIME_CANCEL_MS = 1200; // time editor: longer than LONG_MS so auto-repeat can ramp
constexpr uint8_t  LOW_BATT    = 15;
constexpr int8_t   SIGNAL_BARS = 4;   // placeholder: no NTP/WiFi in this build

const char* const kMenuLabels[(int)MenuItem::Count] = {
    "Time set", "Format", "Theme", "Style", "Brightness", "Back",
};
} // namespace

// ---- one instance each ----------------------------------------------------

M5Canvas canvas(&M5.Display);
WatchFace face;
Stopwatch sw;
Settings settings;
Power power;

Mode     _mode    = Mode::Face;
SetView  _setView = SetView::List;
MenuItem _menuSel = MenuItem::TimeSet;

struct BtnState {
    uint32_t downMs   = 0;
    uint32_t upMs     = 0;
    bool     longDone = false;
    bool     pending  = false;
    bool     swallow  = false;   // 2nd tap of a double: skip its own single tap
};
BtnState _btnA, _btnB;

uint8_t  _batt = 100;
bool     _charging = false;
uint32_t _battReadMs = 0;

uint8_t  _editHour = 0, _editMinute = 0;
uint32_t _repeatDownMs = 0;
uint32_t _repeatLastMs = 0;
bool     _timeLongFired = false;

Settings::Data _editData;
uint32_t _sadUntil = 0;
bool     _dozing = false;

// ---- forward decls --------------------------------------------------------

static void beep(uint16_t freq, uint16_t ms);
static void soundClick();
static void soundConfirm();
static void soundCancel();
static void soundPoke();
static void soundHappy();
static void soundSad();
static void applyLiveSettings();
static const char* eyeName(uint8_t e);
static const char* bodyName(uint8_t b);
static void drawBrightnessBar(M5Canvas* cv, int16_t x, int16_t y, uint8_t level);
static void drawPicker(M5Canvas* cv, const botux::BotUx::Style& st, int16_t cx, int16_t y, const char* value);
static void drawTimeEditor(M5Canvas* cv, const botux::BotUx::Style& st, uint32_t now);
static void drawMenuList(M5Canvas* cv, const botux::BotUx::Style& st);
static const char* editorTitle();
static const char* hintForView();
static void drawEditor(uint32_t now);
static void drawSettings(uint32_t now);
static void drawHints();
static botux::BotUx::Mood resolveMood(uint32_t now);
static void enterSettings();
static void exitSettings();
static void commitTime();
static void incTimeField();
static void handleTimeEditor(uint32_t now);
static void enterEditor(SetView v);
static void liveApplyEdit();
static void saveEdit();
static void enterMenu();
static void handleFaceGestures(Gesture ga, Gesture gb, uint32_t now);
static void handleStopwatchGestures(Gesture ga, Gesture gb, uint32_t now);
static void handleSettingsGestures(Gesture ga, Gesture gb, uint32_t now);
static void handleGestures(Gesture ga, Gesture gb, uint32_t now);
static void refreshPower(uint32_t now);
static void seedRtcIfNeeded();
static void flushButtons();

// ---- button gestures ------------------------------------------------------

template <typename Btn>
static Gesture pollButton(Btn& b, BtnState& s, uint32_t now) {
    if (b.wasPressed()) {
        s.downMs = now;
        s.longDone = false;
        if (s.pending && (now - s.upMs) <= DOUBLE_MS) {
            s.pending = false;
            s.swallow = true;
            return Gesture::Double;
        }
    }
    if (b.isPressed() && !s.longDone && (now - s.downMs) >= LONG_MS) {
        s.longDone = true;
        s.pending = false;
        return Gesture::Long;
    }
    if (b.wasReleased()) {
        if (s.longDone) return Gesture::None;
        if (s.swallow) { s.swallow = false; return Gesture::None; }
        uint32_t held = now - s.downMs;
        if (held < TAP_MS) {
            s.upMs = now;
            s.pending = true;
        }
        return Gesture::None;
    }
    if (s.pending && (now - s.upMs) >= DOUBLE_MS) {
        s.pending = false;
        return Gesture::Tap;
    }
    return Gesture::None;
}

// The time-set editor drives BtnA directly (immediate +1 and auto-repeat), so
// pollButton's per-button state goes stale while it is active. Flush in-flight
// gestures on entry/exit so a mode switch never replays an old edge.
static void flushButtons() {
    _btnA.pending = false;
    _btnA.swallow = false;
    _btnB.pending = false;
    _btnB.swallow = false;
    if (M5.BtnA.isPressed()) _btnA.longDone = true;   // consume the held press
    if (M5.BtnB.isPressed()) _btnB.longDone = true;
}

// ---- audio ----------------------------------------------------------------

static void beep(uint16_t freq, uint16_t ms) { M5.Speaker.tone(freq, ms); }
static void soundClick()   { beep(1000, 20); }
static void soundConfirm() { beep(880, 60); }
static void soundCancel()  { beep(440, 80); }
static void soundPoke()    { beep(1200, 40); }
static void soundHappy()   { beep(1320, 90); }
static void soundSad()     { beep(330, 120); }

// ---- settings helpers -----------------------------------------------------

static const char* eyeName(uint8_t e) {
    static const char* n[4] = { "Round", "Oval", "Square", "Googly" };
    return (e < 4) ? n[e] : "?";
}
static const char* bodyName(uint8_t b) {
    static const char* n[4] = { "None", "Round", "Rounded", "Hexagon" };
    return (b < 4) ? n[b] : "?";
}

// Push the scratch edit into every live consumer (bot style, format, LCD).
static void applyLiveSettings() {
    settings.data() = _editData;
    settings.rebuildStyle();
    settings.apply(face.bot());
    face.setHour24(settings.data().hour24);
    power.applyLevel(settings.data().brightness);
}

// ---- settings drawing -----------------------------------------------------

static void drawBrightnessBar(M5Canvas* cv, int16_t x, int16_t y, uint8_t level) {
    for (uint8_t i = 1; i <= 5; i++) {
        uint16_t c = (i <= level) ? settings.style().accentColor : botux::rgb565(0x30, 0x34, 0x3C);
        cv->fillRoundRect(x + (i - 1) * 9, y, 7, 10, 2, c);
    }
}

static void drawPicker(M5Canvas* cv, const botux::BotUx::Style& st, int16_t cx, int16_t y, const char* value) {
    cv->setTextDatum(middle_center);
    cv->setTextSize(2.0f);
    cv->setTextColor(st.accentColor);
    cv->drawString(value, cx, y);
    cv->setTextColor(st.eyeColor);
    cv->drawString("<", cx - 55, y);
    cv->drawString(">", cx + 55, y);
}

static void drawTimeEditor(M5Canvas* cv, const botux::BotUx::Style& st, uint32_t now) {
    int16_t cx = cv->width() / 2;
    bool blink = ((now / 500) % 2) == 0;

    cv->setTextDatum(middle_center);
    cv->setTextSize(3.0f);

    char buf[4];
    uint8_t hdisp = _editHour;                 // 24 h internally; render 1..12 in 12 h mode
    if (!settings.data().hour24) {
        hdisp = _editHour % 12;
        if (hdisp == 0) hdisp = 12;
    }
    snprintf(buf, sizeof(buf), "%02u", (unsigned)hdisp);
    bool showHour = (_setView != SetView::TimeHour) || blink;
    cv->setTextColor(showHour ? st.accentColor : st.bgColor);
    cv->drawString(buf, cx - 34, 90);

    cv->setTextColor(st.eyeColor);
    cv->drawString(":", cx, 90);

    snprintf(buf, sizeof(buf), "%02u", (unsigned)_editMinute);
    bool showMin = (_setView != SetView::TimeMinute) || blink;
    cv->setTextColor(showMin ? st.accentColor : st.bgColor);
    cv->drawString(buf, cx + 34, 90);
}

static void drawMenuList(M5Canvas* cv, const botux::BotUx::Style& st) {
    for (uint8_t i = 0; i < (uint8_t)MenuItem::Count; i++) {
        int16_t y = 40 + i * 24;
        bool sel = (i == (uint8_t)_menuSel);
        cv->setTextDatum(top_left);
        cv->setTextColor(sel ? st.accentColor : st.eyeColor);
        cv->drawString(sel ? "> " : "  ", 6, y);
        cv->drawString(kMenuLabels[i], 26, y);

        cv->setTextDatum(top_right);
        cv->setTextColor(st.eyeColor);
        switch ((MenuItem)i) {
            case MenuItem::Format:
                cv->drawString(settings.data().hour24 ? "24 h" : "12 h", 131, y);
                break;
            case MenuItem::Theme:
                cv->drawString(Settings::themeName(settings.data().theme), 131, y);
                break;
            case MenuItem::Style:
                cv->drawString(eyeName(settings.data().eyeStyle), 131, y);
                break;
            case MenuItem::Brightness:
                drawBrightnessBar(cv, 88, y, settings.data().brightness);
                break;
            default: break;
        }
    }
}

static const char* editorTitle() {
    switch (_setView) {
        case SetView::TimeHour:
        case SetView::TimeMinute: return "Set time";
        case SetView::Format:     return "Format";
        case SetView::Theme:      return "Theme";
        case SetView::Style:      return "Style";
        case SetView::Brightness: return "Brightness";
        default:                  return "Settings";
    }
}

static const char* hintForView() {
    switch (_setView) {
        case SetView::List:        return "[A] select  [B] next  [long A] back";
        case SetView::TimeHour:
        case SetView::TimeMinute:  return "[A] +1  [B] next  [hold A] repeat";
        case SetView::Style:       return "[A] eye  [B] body  [long A] save";
        case SetView::Brightness:  return "[A] up  [B] down  [long A] save";
        default:                   return "[A] change  [long A] save";
    }
}

static void drawEditor(uint32_t now) {
    M5Canvas* cv = &canvas;
    const botux::BotUx::Style& st = settings.style();
    int16_t cx = cv->width() / 2;

    cv->setTextDatum(top_left);
    cv->setTextSize(1.0f);
    cv->setTextColor(st.eyeColor);
    cv->drawString(editorTitle(), 6, 20);

    switch (_setView) {
        case SetView::TimeHour:
        case SetView::TimeMinute:
            drawTimeEditor(cv, st, now);
            break;
        case SetView::Format:
            drawPicker(cv, st, cx, 96, settings.data().hour24 ? "24 h" : "12 h");
            break;
        case SetView::Theme:
            drawPicker(cv, st, cx, 96, Settings::themeName(settings.data().theme));
            break;
        case SetView::Style:
            cv->setTextDatum(top_left);
            cv->setTextColor(st.accentColor);
            cv->drawString("Eye", 30, 70);
            cv->drawString("Body", 30, 96);
            cv->setTextColor(st.eyeColor);
            cv->drawString(eyeName(settings.data().eyeStyle), 80, 70);
            cv->drawString(bodyName(settings.data().bodyStyle), 80, 96);
            break;
        case SetView::Brightness:
            drawBrightnessBar(cv, cx - 22, 96, settings.data().brightness);
            break;
        default: break;
    }
}

static void drawSettings(uint32_t now) {
    M5Canvas* cv = &canvas;
    const botux::BotUx::Style& st = settings.style();
    cv->fillSprite(st.bgColor);

    cv->setTextDatum(top_left);
    cv->setTextSize(1.0f);
    cv->setTextColor(st.accentColor);
    cv->drawString("Settings", 6, 6);

    if (_setView == SetView::List) drawMenuList(cv, st);
    else drawEditor(now);

    cv->setTextDatum(bottom_left);
    cv->setTextColor(st.eyeColor);
    cv->drawString(hintForView(), 6, 234);
}

static void drawHints() {
    canvas.setTextDatum(bottom_center);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.style().eyeColor);
    const char* hint = (_mode == Mode::Stopwatch)
        ? "[A] start/lap/reset  [B] stop/resume"
        : "[A] poke  [B] stopwatch";
    canvas.drawString(hint, 67, 236);
}

// ---- mood ----------------------------------------------------------------

static botux::BotUx::Mood resolveMood(uint32_t now) {
    if (_dozing)                return botux::BotUx::Mood::Sleepy;
    if (_batt <= LOW_BATT)      return botux::BotUx::Mood::Sleepy;
    if (_charging)              return botux::BotUx::Mood::Happy;
    if (now < _sadUntil)        return botux::BotUx::Mood::Sad;
    switch (_mode) {
        case Mode::Stopwatch:   return sw.running() ? botux::BotUx::Mood::Happy : botux::BotUx::Mood::Idle;
        case Mode::Settings:    return botux::BotUx::Mood::Listening;
        default:                return botux::BotUx::Mood::Idle;
    }
}

// ---- mode transitions ----------------------------------------------------

static void enterSettings() {
    _mode = Mode::Settings;
    _setView = SetView::List;
    _menuSel = MenuItem::TimeSet;
    soundConfirm();
}

static void exitSettings() {
    _mode = Mode::Face;
    _setView = SetView::List;
    _menuSel = MenuItem::TimeSet;
    soundCancel();
}

static void commitTime() {
    auto dt = M5.Rtc.getDateTime();
    dt.time.hours = (int8_t)_editHour;
    dt.time.minutes = (int8_t)_editMinute;
    dt.time.seconds = 0;
    M5.Rtc.setDateTime(dt);
}

static void incTimeField() {
    if (_setView == SetView::TimeHour) _editHour = (_editHour + 1) % 24;
    else                               _editMinute = (_editMinute + 1) % 60;
}

static void handleTimeEditor(uint32_t now) {
    // A: +1 immediately on press, auto-repeat while held, long cancels.
    if (M5.BtnA.wasPressed()) {
        _repeatDownMs = now;
        _repeatLastMs = now;
        _timeLongFired = false;
        incTimeField();
        soundClick();
    }
    if (M5.BtnA.isPressed()) {
        uint32_t held = now - _repeatDownMs;
        if (held >= TIME_CANCEL_MS) {
            if (!_timeLongFired) {
                _timeLongFired = true;
                soundCancel();
                flushButtons();
                _setView = SetView::List;
            }
        } else if (held >= 300) {
            // accelerate 8 Hz -> 20 Hz over the first second
            uint32_t t = (held < 1000) ? held : 1000;
            float hz = 8.0f + 12.0f * (float)t / 1000.0f;
            uint32_t interval = (uint32_t)(1000.0f / hz);
            if (now - _repeatLastMs >= interval) {
                _repeatLastMs = now;
                incTimeField();
            }
        }
    }
    // B: hour -> minute -> done
    if (M5.BtnB.wasPressed()) {
        soundClick();
        if (_setView == SetView::TimeHour) _setView = SetView::TimeMinute;
        else { commitTime(); flushButtons(); _setView = SetView::List; soundConfirm(); }
    }
}

// ---- settings editors (non-time) -----------------------------------------

static void enterEditor(SetView v) {
    _setView = v;
    _editData = settings.data();
    if (v == SetView::TimeHour) {
        _editHour = face.hour();
        _editMinute = face.minute();
        _repeatDownMs = 0;
        _repeatLastMs = 0;
        _timeLongFired = false;
        flushButtons();
    }
}

static void liveApplyEdit() { applyLiveSettings(); }

static void saveEdit() {
    settings.save();
    soundConfirm();
    _setView = SetView::List;
}

static void enterMenu() {
    switch (_menuSel) {
        case MenuItem::TimeSet:     enterEditor(SetView::TimeHour); break;
        case MenuItem::Format:      enterEditor(SetView::Format); break;
        case MenuItem::Theme:       enterEditor(SetView::Theme); break;
        case MenuItem::Style:       enterEditor(SetView::Style); break;
        case MenuItem::Brightness:  enterEditor(SetView::Brightness); break;
        case MenuItem::Back:        exitSettings(); break;
        default: break;
    }
}

// ---- gesture routing ------------------------------------------------------

static void handleFaceGestures(Gesture ga, Gesture gb, uint32_t now) {
    (void)now;
    if (ga == Gesture::Tap) {
        face.bot().poke();
        soundPoke();
    } else if (ga == Gesture::Double) {
        settings.data().hour24 = !settings.data().hour24;
        settings.save();
        face.setHour24(settings.data().hour24);
        soundConfirm();
    } else if (ga == Gesture::Long) {
        enterSettings();
    }
    if (gb == Gesture::Tap) {
        _mode = Mode::Stopwatch;
        soundHappy();
    } else if (gb == Gesture::Long) {
        _dozing = true;
        power.setBrightness(76);   // ~30% — dim doze
        soundCancel();
    }
}

static void handleStopwatchGestures(Gesture ga, Gesture gb, uint32_t now) {
    if (ga == Gesture::Tap) {
        Stopwatch::State before = sw.state();
        sw.pressPrimary();
        if (before == Stopwatch::State::Idle)        { soundClick(); }                       // start
        else if (before == Stopwatch::State::Running){ face.bot().poke(); soundClick(); }    // lap
        else                                         { _sadUntil = now + 700; soundSad(); }  // reset
    } else if (ga == Gesture::Long) {
        soundCancel();
        _mode = Mode::Face;
    }
    if (gb == Gesture::Tap) {
        Stopwatch::State before = sw.state();
        sw.pressSecondary();
        if (before == Stopwatch::State::Running) soundClick();    // stop
        else if (before == Stopwatch::State::Stopped) soundHappy(); // resume
    }
}

static void handleSettingsGestures(Gesture ga, Gesture gb, uint32_t now) {
    (void)now;
    if (_setView == SetView::List) {
        if (ga == Gesture::Tap)        { soundClick(); enterMenu(); }
        else if (ga == Gesture::Long)  { exitSettings(); }
        if (gb == Gesture::Tap)        { soundClick(); _menuSel = (MenuItem)(((uint8_t)_menuSel + 1) % (uint8_t)MenuItem::Count); }
        return;
    }

    switch (_setView) {
        case SetView::Format:
            if (ga == Gesture::Tap) { _editData.hour24 = !_editData.hour24; liveApplyEdit(); soundClick(); }
            else if (ga == Gesture::Long) { saveEdit(); }
            break;
        case SetView::Theme:
            if (ga == Gesture::Tap) { _editData.theme = (_editData.theme + 1) % Settings::THEME_COUNT; liveApplyEdit(); soundClick(); }
            else if (ga == Gesture::Long) { saveEdit(); }
            break;
        case SetView::Style:
            if (ga == Gesture::Tap) { _editData.eyeStyle = (_editData.eyeStyle + 1) % 4; liveApplyEdit(); soundClick(); }
            else if (ga == Gesture::Long) { saveEdit(); }
            if (gb == Gesture::Tap) { _editData.bodyStyle = (_editData.bodyStyle + 1) % 4; liveApplyEdit(); soundClick(); }
            break;
        case SetView::Brightness:
            if (ga == Gesture::Tap) { _editData.brightness = (_editData.brightness % 5) + 1; liveApplyEdit(); soundClick(); }
            else if (ga == Gesture::Long) { saveEdit(); }
            if (gb == Gesture::Tap) { _editData.brightness = (_editData.brightness == 1) ? 5 : (_editData.brightness - 1); liveApplyEdit(); soundClick(); }
            break;
        default: break;
    }
}

static void handleGestures(Gesture ga, Gesture gb, uint32_t now) {
    switch (_mode) {
        case Mode::Face:      handleFaceGestures(ga, gb, now); break;
        case Mode::Stopwatch: handleStopwatchGestures(ga, gb, now); break;
        case Mode::Settings:  handleSettingsGestures(ga, gb, now); break;
    }
}

// ---- power + rtc ----------------------------------------------------------

static void refreshPower(uint32_t now) {
    if (now - _battReadMs >= 1000) {
        _battReadMs = now;
        _batt = power.batteryPct();
        _charging = power.charging();
    }
}

static void seedRtcIfNeeded() {
    auto dt = M5.Rtc.getDateTime();
    if (dt.date.year >= 2020) return;   // already plausible

    // BM8563 has no coin cell on the StickC — seed from compile time.
    int y = 2026, mo = 1, d = 1, h = 0, mi = 0, s = 0;
    char mon[4];
    sscanf(__DATE__, "%3s %d %d", mon, &d, &y);
    const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* p = strstr(months, mon);
    if (p) mo = (int)((p - months) / 3 + 1);
    sscanf(__TIME__, "%d:%d:%d", &h, &mi, &s);

    auto out = M5.Rtc.getDateTime();
    out.date.year = (int16_t)y;
    out.date.month = (int8_t)mo;
    out.date.date = (int8_t)d;
    out.time.hours = (int8_t)h;
    out.time.minutes = (int8_t)mi;
    out.time.seconds = (int8_t)s;
    M5.Rtc.setDateTime(out);
}

// ---- setup / loop ---------------------------------------------------------

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    power.begin();
    settings.begin();

    canvas.setColorDepth(16);
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    face.begin(&canvas);
    sw.begin();

    settings.rebuildStyle();
    settings.apply(face.bot());
    face.setHour24(settings.data().hour24);
    power.applyLevel(settings.data().brightness);

    seedRtcIfNeeded();

    _batt = power.batteryPct();
    _charging = power.charging();
    _battReadMs = millis();
}

void loop() {
    M5.update();
    uint32_t now = millis();

    refreshPower(now);

    if (_dozing) {
        if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
            _dozing = false;
            flushButtons();
            power.applyLevel(settings.data().brightness);
            soundConfirm();
        }
    } else {
        bool timeEdit = (_setView == SetView::TimeHour || _setView == SetView::TimeMinute);
        if (_mode == Mode::Settings && timeEdit) {
            handleTimeEditor(now);
        } else {
            Gesture ga = pollButton(M5.BtnA, _btnA, now);
            Gesture gb = pollButton(M5.BtnB, _btnB, now);
            handleGestures(ga, gb, now);
        }
    }

    face.setBattery(_batt);
    face.setCharging(_charging);
    face.setSignal(SIGNAL_BARS);
    face.bot().setMood(resolveMood(now));
    face.update(now);
    sw.update(now);

    face.bot().update(now);
    if (_mode == Mode::Settings) {
        drawSettings(now);
    } else {
        face.bot().draw();
        if (_mode == Mode::Stopwatch) {
            sw.draw(&canvas, settings.style().accentColor, settings.style().eyeColor);
        } else {
            face.draw();
        }
        drawHints();
    }

    canvas.pushSprite(0, 0);
    M5.Display.waitDisplay();
}
