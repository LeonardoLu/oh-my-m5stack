// bot-ux-watch — M5Stack StopWatch (ESP32-S3, 466x466 round AMOLED touch).
//
// main.cpp owns the mode state machine (Face / Stopwatch / Settings), the touch
// + button gesture routing, and fixed-buffer rendering. The shared bot-ux bot is
// the Face hero; stopwatch and settings are utilitarian touch screens. Modules
// are plain classes, one instance each; no heap in the loop.

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
enum class SetView : uint8_t { List, TimeHour, TimeMinute, Format, Theme, Style, Brightness };
enum class MenuItem : uint8_t { TimeSet = 0, Format, Theme, Style, Brightness, Back, Count };

// Buttons produce Tap/Double/Long; touch adds SwipeUp/SwipeDown.
enum class Gesture : uint8_t { None, Tap, Double, Long, SwipeUp, SwipeDown };

namespace {
constexpr uint32_t LONG_MS      = 600;
constexpr uint32_t TAP_MS       = 250;
constexpr uint32_t DOUBLE_MS    = 300;
constexpr uint32_t TIME_CANCEL_MS = 1200; // time editor: > LONG_MS so auto-repeat can ramp
constexpr uint8_t  LOW_BATT     = 15;
constexpr int8_t   SIGNAL_BARS  = 4;   // placeholder: no NTP/WiFi in this build

// ---- layout (466x466 round AMOLED) ----
constexpr int16_t kW = 466, kH = 466;
constexpr int16_t kBotW = 200, kBotH = 200;
constexpr int16_t kBotX = 133, kBotY = 15;   // (466-200)/2, bot body ~ y 15..215
constexpr int16_t kMenuTop = 120, kMenuStep = 52;  // settings list rows

const char* const kMenuLabels[(int)MenuItem::Count] = {
    "Time set", "Format", "Theme", "Style", "Brightness", "Back",
};
} // namespace

// ---- one instance each ----------------------------------------------------

M5Canvas canvas(&M5.Display);
M5Canvas botSprite(&M5.Display);
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

// ---- touch state ----------------------------------------------------------
uint32_t _tDownMs = 0;
int16_t  _tx0 = 0, _ty0 = 0, _tx = 0, _ty = 0, _tapX = 0, _tapY = 0;
bool     _tDown = false, _tMoved = false, _tLong = false, _tSwiped = false;

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
static void pokeBot();
static void gotoStopwatch();
static void startDoze();
static void handleFaceButtons(Gesture ga, Gesture gb, uint32_t now);
static void handleStopwatchButtons(Gesture ga, Gesture gb, uint32_t now);
static void handleSettingsButtons(Gesture ga, Gesture gb, uint32_t now);
static void handleButtons(Gesture ga, Gesture gb, uint32_t now);
static void handleTouch(Gesture g, uint32_t now);
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

static void flushButtons() {
    _btnA.pending = false;
    _btnA.swallow = false;
    _btnB.pending = false;
    _btnB.swallow = false;
    if (M5.BtnA.isPressed()) _btnA.longDone = true;   // consume the held press
    if (M5.BtnB.isPressed()) _btnB.longDone = true;
}

// ---- touch gestures -------------------------------------------------------

// Single-point touch → Tap (with _tapX/_tapY), SwipeUp, SwipeDown, or Long.
static Gesture pollTouch(uint32_t now) {
    auto t = M5.Touch.getDetail(0);
    Gesture g = Gesture::None;

    if (t.wasPressed()) {
        _tDown = true; _tMoved = false; _tLong = false; _tSwiped = false;
        _tx0 = _tx = (int16_t)t.x; _ty0 = _ty = (int16_t)t.y;
        _tDownMs = now;
    } else if (t.isPressed() && _tDown) {
        _tx = (int16_t)t.x; _ty = (int16_t)t.y;
        if (!_tMoved && (abs(_tx - _tx0) > 24 || abs(_ty - _ty0) > 24)) _tMoved = true;
        if (!_tLong && !_tMoved && !_tSwiped && (now - _tDownMs) >= LONG_MS) {
            _tLong = true;
            g = Gesture::Long;
        }
        if (_tMoved && !_tSwiped) {
            int16_t dx = _tx - _tx0, dy = _ty - _ty0;
            if (dy < -50 && abs(dy) > abs(dx))      { _tSwiped = true; g = Gesture::SwipeUp; }
            else if (dy > 50 && abs(dy) > abs(dx))  { _tSwiped = true; g = Gesture::SwipeDown; }
        }
    } else if (t.wasReleased() && _tDown) {
        _tDown = false;
        if (!_tLong && !_tMoved && !_tSwiped) {
            _tapX = _tx; _tapY = _ty;
            g = Gesture::Tap;
        }
    }
    return g;
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
        cv->fillRoundRect(x + (i - 1) * 18, y - 6, 14, 12, 3, c);
    }
}

static void drawPicker(M5Canvas* cv, const botux::BotUx::Style& st, int16_t cx, int16_t y, const char* value) {
    cv->setTextDatum(middle_center);
    cv->setTextSize(2.5f);
    cv->setTextColor(st.accentColor);
    cv->drawString(value, cx, y);
    cv->setTextSize(2.0f);
    cv->setTextColor(st.eyeColor);
    cv->drawString("<", cx - 130, y);
    cv->drawString(">", cx + 130, y);
}

static void drawTimeEditor(M5Canvas* cv, const botux::BotUx::Style& st, uint32_t now) {
    int16_t cx = cv->width() / 2;
    bool blink = ((now / 500) % 2) == 0;

    cv->setTextDatum(middle_center);
    cv->setTextSize(4.0f);

    char buf[4];
    uint8_t hdisp = _editHour;                 // 24 h internally; render 1..12 in 12 h mode
    if (!settings.data().hour24) {
        hdisp = _editHour % 12;
        if (hdisp == 0) hdisp = 12;
    }
    snprintf(buf, sizeof(buf), "%02u", (unsigned)hdisp);
    bool showHour = (_setView != SetView::TimeHour) || blink;
    cv->setTextColor(showHour ? st.accentColor : st.bgColor);
    cv->drawString(buf, cx - 70, 200);

    cv->setTextColor(st.eyeColor);
    cv->drawString(":", cx, 200);

    snprintf(buf, sizeof(buf), "%02u", (unsigned)_editMinute);
    bool showMin = (_setView != SetView::TimeMinute) || blink;
    cv->setTextColor(showMin ? st.accentColor : st.bgColor);
    cv->drawString(buf, cx + 70, 200);
}

static void drawMenuList(M5Canvas* cv, const botux::BotUx::Style& st) {
    for (uint8_t i = 0; i < (uint8_t)MenuItem::Count; i++) {
        int16_t y = kMenuTop + i * kMenuStep;
        bool sel = (i == (uint8_t)_menuSel);
        if (sel) cv->fillRoundRect(40, y - 20, 386, 40, 10, st.bodyColor);

        cv->setTextDatum(middle_left);
        cv->setTextSize(1.5f);
        cv->setTextColor(sel ? st.accentColor : st.eyeColor);
        cv->drawString(kMenuLabels[i], 60, y);

        cv->setTextDatum(middle_right);
        cv->setTextColor(st.eyeColor);
        switch ((MenuItem)i) {
            case MenuItem::Format:
                cv->drawString(settings.data().hour24 ? "24 h" : "12 h", 406, y);
                break;
            case MenuItem::Theme:
                cv->drawString(Settings::themeName(settings.data().theme), 406, y);
                break;
            case MenuItem::Style:
                cv->drawString(eyeName(settings.data().eyeStyle), 406, y);
                break;
            case MenuItem::Brightness:
                drawBrightnessBar(cv, 300, y, settings.data().brightness);
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
        case SetView::List:        return "tap select · swipe down back · [B] next";
        case SetView::TimeHour:
        case SetView::TimeMinute:  return "tap +1 · [B] next · swipe down save";
        case SetView::Style:       return "tap eye · [B] body · swipe down save";
        case SetView::Brightness:  return "tap up · [B] down · swipe down save";
        default:                   return "tap change · swipe down save";
    }
}

static void drawEditor(uint32_t now) {
    M5Canvas* cv = &canvas;
    const botux::BotUx::Style& st = settings.style();
    int16_t cx = cv->width() / 2;

    cv->setTextDatum(top_left);
    cv->setTextSize(1.5f);
    cv->setTextColor(st.eyeColor);
    cv->drawString(editorTitle(), 60, 60);

    switch (_setView) {
        case SetView::TimeHour:
        case SetView::TimeMinute:
            drawTimeEditor(cv, st, now);
            break;
        case SetView::Format:
            drawPicker(cv, st, cx, 200, settings.data().hour24 ? "24 h" : "12 h");
            break;
        case SetView::Theme:
            drawPicker(cv, st, cx, 200, Settings::themeName(settings.data().theme));
            break;
        case SetView::Style:
            cv->setTextDatum(middle_left);
            cv->setTextSize(2.0f);
            cv->setTextColor(st.accentColor);
            cv->drawString("Eye", 90, 180);
            cv->drawString("Body", 90, 230);
            cv->setTextColor(st.eyeColor);
            cv->drawString(eyeName(settings.data().eyeStyle), 200, 180);
            cv->drawString(bodyName(settings.data().bodyStyle), 200, 230);
            break;
        case SetView::Brightness:
            drawBrightnessBar(cv, cx - 40, 200, settings.data().brightness);
            break;
        default: break;
    }
}

static void drawSettings(uint32_t now) {
    M5Canvas* cv = &canvas;
    const botux::BotUx::Style& st = settings.style();
    cv->fillSprite(st.bgColor);

    cv->setTextDatum(middle_center);
    cv->setTextSize(2.0f);
    cv->setTextColor(st.accentColor);
    cv->drawString("Settings", cv->width() / 2, 40);

    if (_setView == SetView::List) drawMenuList(cv, st);
    else drawEditor(now);

    cv->setTextDatum(bottom_center);
    cv->setTextSize(1.0f);
    cv->setTextColor(st.eyeColor);
    cv->drawString(hintForView(), cv->width() / 2, 450);
}

static void drawHints() {
    canvas.setTextDatum(bottom_center);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.style().eyeColor);
    const char* hint = (_mode == Mode::Stopwatch)
        ? "tap start/lap/reset · [B] stop/resume · swipe down back"
        : "tap bot poke · swipe up settings · [B] stopwatch";
    canvas.drawString(hint, canvas.width() / 2, 450);
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

// ---- mode transitions + actions ------------------------------------------

static void enterSettings() { _mode = Mode::Settings; _setView = SetView::List; _menuSel = MenuItem::TimeSet; soundConfirm(); }
static void exitSettings()  { _mode = Mode::Face; _setView = SetView::List; _menuSel = MenuItem::TimeSet; soundCancel(); }
static void pokeBot()       { face.bot().poke(); soundPoke(); }
static void gotoStopwatch() { _mode = Mode::Stopwatch; soundHappy(); }

static void startDoze() {
    _dozing = true;
    power.setBrightness(76);   // ~30% — dim doze
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

// ---- button routing -------------------------------------------------------

static void handleFaceButtons(Gesture ga, Gesture gb, uint32_t now) {
    (void)now;
    if (ga == Gesture::Tap) {
        pokeBot();
    } else if (ga == Gesture::Double) {
        settings.data().hour24 = !settings.data().hour24;
        settings.save();
        face.setHour24(settings.data().hour24);
        soundConfirm();
    } else if (ga == Gesture::Long) {
        enterSettings();
    }
    if (gb == Gesture::Tap) {
        gotoStopwatch();
    } else if (gb == Gesture::Long) {
        startDoze();
    }
}

static void handleStopwatchButtons(Gesture ga, Gesture gb, uint32_t now) {
    if (ga == Gesture::Tap) {
        Stopwatch::State before = sw.state();
        sw.pressPrimary();
        if (before == Stopwatch::State::Idle)        { soundClick(); }
        else if (before == Stopwatch::State::Running){ pokeBot(); soundClick(); }
        else                                         { _sadUntil = now + 700; soundSad(); }
    } else if (ga == Gesture::Long) {
        soundCancel();
        _mode = Mode::Face;
    }
    if (gb == Gesture::Tap) {
        Stopwatch::State before = sw.state();
        sw.pressSecondary();
        if (before == Stopwatch::State::Running) soundClick();
        else if (before == Stopwatch::State::Stopped) soundHappy();
    }
}

static void handleSettingsButtons(Gesture ga, Gesture gb, uint32_t now) {
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

static void handleButtons(Gesture ga, Gesture gb, uint32_t now) {
    switch (_mode) {
        case Mode::Face:      handleFaceButtons(ga, gb, now); break;
        case Mode::Stopwatch: handleStopwatchButtons(ga, gb, now); break;
        case Mode::Settings:  handleSettingsButtons(ga, gb, now); break;
    }
}

// ---- touch routing --------------------------------------------------------

static void handleTouch(Gesture g, uint32_t now) {
    switch (_mode) {
        case Mode::Face:
            if (g == Gesture::Tap) {
                if (_tapY < 250) pokeBot(); else gotoStopwatch();
            } else if (g == Gesture::SwipeUp) {
                enterSettings();
            } else if (g == Gesture::SwipeDown || g == Gesture::Long) {
                startDoze();
            }
            break;

        case Mode::Stopwatch:
            if (g == Gesture::Tap) {
                handleStopwatchButtons(Gesture::Tap, Gesture::None, now);
            } else if (g == Gesture::SwipeDown || g == Gesture::Long) {
                soundCancel();
                _mode = Mode::Face;
            }
            break;

        case Mode::Settings:
            if (g == Gesture::Tap) {
                if (_setView == SetView::List) {
                    for (int i = 0; i < (int)MenuItem::Count; i++) {
                        int16_t ry = kMenuTop + i * kMenuStep;
                        if (_tapY >= ry - 20 && _tapY < ry + 20) {
                            _menuSel = (MenuItem)i;
                            soundClick();
                            enterMenu();
                            break;
                        }
                    }
                } else {
                    // editor: tap advances the value (same as button A)
                    handleSettingsButtons(Gesture::Tap, Gesture::None, now);
                }
            } else if (g == Gesture::SwipeDown) {
                if (_setView == SetView::List) exitSettings(); else saveEdit();
            } else if (g == Gesture::Long) {
                if (_setView == SetView::List) exitSettings(); else saveEdit();
            }
            break;
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
    canvas.createSprite(kW, kH);
    botSprite.setColorDepth(16);
    botSprite.createSprite(kBotW, kBotH);

    face.begin(&canvas, &botSprite);
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
        // wake on any button or touch
        if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.Touch.getDetail(0).wasPressed()) {
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
            handleButtons(ga, gb, now);
        }
        Gesture gt = pollTouch(now);
        if (gt != Gesture::None) handleTouch(gt, now);
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
        canvas.fillSprite(settings.style().bgColor);
        face.bot().draw();                       // into botSprite
        botSprite.pushSprite(&canvas, kBotX, kBotY);
        if (_mode == Mode::Stopwatch) {
            canvas.setTextDatum(middle_center);
            canvas.setTextSize(1.5f);
            canvas.setTextColor(settings.style().eyeColor);
            canvas.drawString("STOPWATCH", kW / 2, 40);
            sw.draw(&canvas, settings.style().accentColor, settings.style().eyeColor);
        } else {
            face.draw();                          // clock + status (over bot sprite corners)
        }
        drawHints();
    }

    canvas.pushSprite(0, 0);
    M5.Display.waitDisplay();
}
