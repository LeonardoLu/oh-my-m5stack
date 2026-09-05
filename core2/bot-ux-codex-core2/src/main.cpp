// bot-ux-codex-core2 — codex prompt app for the M5Stack Core2.
//
// Owns the frame loop, touch dispatch, the prompt buffer, the device clock, and the
// single botux::BotUx instance. Modules: CodexKeyboard (keys), ChatView (transcript +
// mood wiring), Settings (prefs + screen), Haptics (beeps). The bot renders into a
// small 72px sprite and is pushSprite()'d onto the full-screen canvas each frame.

#include <M5Unified.h>

#include "BotUx.h"
#include "ChatView.h"
#include "CodexKeyboard.h"
#include "Haptics.h"
#include "Settings.h"

#include <stdio.h>  // snprintf
#include <string.h> // strlen

// ---- screen layout (320x240 landscape) ------------------------------------
constexpr int16_t kScreenW = 320;
constexpr int16_t kScreenH = 240;
constexpr int16_t kStatusH = 24;
constexpr int16_t kBotX = 0, kBotY = 24, kBotW = 72, kBotH = 72; // bot tile
constexpr int16_t kPromptY = 96, kPromptH = 32;
constexpr int16_t kKbY = 128; // keyboard top

constexpr int kPromptLen = 48;

// ---- modules (one instance each) ------------------------------------------
M5Canvas _canvas(&M5.Display);
M5Canvas _botSprite(&M5.Display);
botux::BotUx _bot;
CodexKeyboard _kb;
ChatView _chat;
Settings _settings;
Haptics _haptics;

// ---- prompt + device state ------------------------------------------------
char _prompt[kPromptLen];
int  _promptLen = 0;

char _clockBuf[8];   // "HH:MM"
char _statusBuf[24]; // "bat 87% - no signal"
uint32_t _epochMs = 0;      // millis() at boot
uint32_t _timeBaseMs = 0;   // ms-of-day at boot (from __TIME__)

// ---- touch gesture state ---------------------------------------------------
enum Zone : uint8_t { ZONE_OTHER, ZONE_BOT, ZONE_KEY, ZONE_PROMPT, ZONE_SETTINGS };

uint32_t _now = 0;
int16_t _px = 0, _py = 0, _lastX = 0, _lastY = 0;
uint32_t _pMs = 0;
bool _down = false;
bool _moved = false;
bool _swiped = false;
bool _longFired = false;
int16_t _curKey = -1;
Zone _zone = ZONE_OTHER;
uint32_t _napUntil = 0;
uint32_t _lastPowerMs = 0;

// settings-change trackers (for apply + feedback)
int _lastTheme = -1, _lastBright = -1, _lastHaptics = -1, _lastLayout = -1;

// ---- helpers ---------------------------------------------------------------
static uint32_t compileTimeMs()
{
    // __TIME__ = "HH:MM:SS" — a deterministic boot time, no RTC dependency.
    uint8_t h = (uint8_t)((__TIME__[0] - '0') * 10 + (__TIME__[1] - '0'));
    uint8_t m = (uint8_t)((__TIME__[3] - '0') * 10 + (__TIME__[4] - '0'));
    uint8_t s = (uint8_t)((__TIME__[6] - '0') * 10 + (__TIME__[7] - '0'));
    return ((uint32_t)h * 3600u + (uint32_t)m * 60u + (uint32_t)s) * 1000u;
}

static void updateClock()
{
    // mod each term first so the sum can't overflow uint32 on long uptimes
    uint32_t elapsed = (_now - _epochMs) % 86400000uL;
    uint32_t tod = (_timeBaseMs + elapsed) % 86400000uL;
    uint8_t hh = (uint8_t)(tod / 3600000uL);
    uint8_t mm = (uint8_t)((tod % 3600000uL) / 60000uL);
    snprintf(_clockBuf, sizeof(_clockBuf), "%02u:%02u", hh, mm);
}

static bool inBot(int16_t x, int16_t y)
{
    const botux::BotUx::Metrics& m = _bot.metrics();
    int16_t cx = kBotX + m.cx;
    int16_t cy = kBotY + m.cy;
    int16_t r = (m.bodyR > 0) ? m.bodyR : m.eyeRadius * 3;
    return (x >= cx - r && x <= cx + r && y >= cy - r && y <= cy + r);
}

static void setPrompt(const char* s)
{
    int n = 0;
    while (s[n] && n < kPromptLen - 1) { _prompt[n] = s[n]; n++; }
    _prompt[n] = 0;
    _promptLen = n;
}

static void submitPrompt()
{
    if (_promptLen == 0) return;
    _chat.submit(_prompt);
    _promptLen = 0;
    _prompt[0] = 0;
    _haptics.sendTone();
    _haptics.vibrate(60);
}

static void applyKey(int16_t id)
{
    switch (_kb.kind(id))
    {
        case CodexKeyboard::K_CHAR:
        {
            char c = _kb.keyChar(id);
            if (c && _promptLen < kPromptLen - 1)
            {
                _prompt[_promptLen++] = c;
                _prompt[_promptLen] = 0;
            }
            _chat.setListening();
            _haptics.keyTick();
            break;
        }
        case CodexKeyboard::K_SPACE:
            if (_promptLen < kPromptLen - 1)
            {
                _prompt[_promptLen++] = ' ';
                _prompt[_promptLen] = 0;
            }
            _chat.setListening();
            _haptics.keyTick();
            break;
        case CodexKeyboard::K_BACKSPACE:
            if (_promptLen > 0) _prompt[--_promptLen] = 0;
            _haptics.keyTick();
            break;
        case CodexKeyboard::K_SHIFT:
            _kb.toggleShift();
            _haptics.click();
            break;
        case CodexKeyboard::K_LAYER:
            _kb.toggleLayer();
            _haptics.click();
            break;
        case CodexKeyboard::K_CHIP:
        {
            const char* t = _kb.keyText(id);
            if (t) setPrompt(t);
            _chat.setListening();
            _haptics.click();
            break;
        }
        case CodexKeyboard::K_ENTER:
        case CodexKeyboard::K_SEND:
            submitPrompt();
            break;
        default:
            break;
    }
}

// ---- touch dispatch --------------------------------------------------------
static void touchBegin(int16_t x, int16_t y)
{
    _down = true;
    _moved = false;
    _swiped = false;
    _longFired = false;
    _px = x; _py = y; _lastX = x; _lastY = y;
    _pMs = _now;
    _curKey = -1;

    if (_settings.isOpen())
    {
        _zone = ZONE_SETTINGS;
        _settings.touchBegin(x, y);
        return;
    }
    if (inBot(x, y))
    {
        _zone = ZONE_BOT;
    }
    else if (y >= kPromptY && y < kPromptY + kPromptH)
    {
        _zone = ZONE_PROMPT;
        _chat.setListening();
        _haptics.click();
    }
    else if (_kb.contains(y))
    {
        _zone = ZONE_KEY;
        _curKey = _kb.hitTest(x, y);
        if (_curKey >= 0) applyKey(_curKey); // fire on press for snappy typing
    }
    else
    {
        _zone = ZONE_OTHER;
    }
}

static void touchMove(int16_t x, int16_t y)
{
    if (!_down) return;
    _lastX = x; _lastY = y;

    if (_zone == ZONE_SETTINGS)
    {
        _settings.touchMove(x, y);
        return;
    }
    if (_zone == ZONE_BOT)
    {
        int16_t dx = x - _px;
        int16_t dy = y - _py;
        if (abs(dx) > 15 || abs(dy) > 15) _moved = true; // tap-cancel latch only
        if (!_swiped)
        {
            if (abs(dx) >= abs(dy))
            {
                // theme-cycle feedback is emitted by syncSettings() once the value changes
                if (dx > 40)       { _settings.cycleTheme(+1); _swiped = true; _moved = true; }
                else if (dx < -40) { _settings.cycleTheme(-1); _swiped = true; _moved = true; }
            }
            else if (dy < -40)
            {
                _settings.open();
                _zone = ZONE_SETTINGS;
                _swiped = true;
                _moved = true;
                _haptics.confirm();
            }
        }
    }
}

static void touchEnd(int16_t x, int16_t y)
{
    if (!_down) return;
    int16_t ex = _lastX, ey = _lastY; // lift-off may report (0,0)
    uint32_t dur = _now - _pMs;
    (void)x; (void)y;

    if (_zone == ZONE_SETTINGS)
    {
        _settings.touchEnd(ex, ey);
    }
    else if (_zone == ZONE_BOT)
    {
        if (!_moved && !_longFired && dur < 250)
        {
            _bot.poke();
            _haptics.poke();
        }
    }
    // ZONE_KEY: key already applied on press; highlight clears below.

    _down = false;
    _zone = ZONE_OTHER;
    _curKey = -1;
}

static void handleTouch()
{
    auto d = M5.Touch.getDetail(0);
    if (d.wasPressed())
        touchBegin((int16_t)d.x, (int16_t)d.y);
    else if (d.isPressed())
        touchMove((int16_t)d.x, (int16_t)d.y);
    else if (d.wasReleased())
        touchEnd((int16_t)d.x, (int16_t)d.y);
}

// ---- settings sync ---------------------------------------------------------
static void syncSettings()
{
    const Settings::Data& d = _settings.data();

    if (_lastTheme != d.theme)
    {
        _lastTheme = d.theme;
        _bot.setStyle(Settings::themeStyle(d.theme));
        _haptics.click();
    }
    if (_lastHaptics != d.haptics)
    {
        _lastHaptics = d.haptics;
        _haptics.setEnabled(d.haptics);
        if (d.haptics) _haptics.confirm();
    }
    if (_lastBright != d.brightness)
    {
        _lastBright = d.brightness; // Settings already set the display
        _haptics.click();
    }
    if (_lastLayout != d.kbdLayout)
    {
        _lastLayout = d.kbdLayout;
        _kb.setChipsOnly(d.kbdLayout == 1);
        _haptics.click();
    }
}

// ---- drawing ---------------------------------------------------------------
static void drawPrompt(const botux::BotUx::Style& st)
{
    _canvas.fillRoundRect(4, kPromptY + 2, kScreenW - 8, kPromptH - 4, 6, st.bodyColor);

    const char* start = _prompt;
    if (_promptLen > 28) start = _prompt + (_promptLen - 28);
    char buf[kPromptLen + 4];
    snprintf(buf, sizeof(buf), "> %s%s", start, ((_now / 500) & 1) ? "|" : "");

    _canvas.setTextDatum(middle_left);
    _canvas.setTextSize(1.0f);
    _canvas.setTextColor(st.accentColor);
    _canvas.drawString(buf, 12, kPromptY + kPromptH / 2);
}

static void drawMain()
{
    const botux::BotUx::Style& st = _bot.style();
    _canvas.fillSprite(st.bgColor);

    // status bar: clock · title · battery
    _canvas.setTextDatum(middle_left);
    _canvas.setTextSize(1.0f);
    _canvas.setTextColor(st.accentColor);
    _canvas.drawString(_clockBuf, 6, kStatusH / 2);
    _canvas.setTextDatum(middle_center);
    _canvas.setTextColor(st.bodyColor);
    _canvas.drawString("codex", kScreenW / 2, kStatusH / 2);
    _canvas.setTextDatum(middle_right);
    _canvas.setTextColor(st.eyeColor);
    _canvas.drawString(_statusBuf, kScreenW - 6, kStatusH / 2);
    _canvas.drawLine(0, kStatusH - 1, kScreenW, kStatusH - 1, st.bodyColor);

    // bot tile (already rendered into _botSprite)
    _botSprite.pushSprite(&_canvas, kBotX, kBotY);

    _chat.draw();
    drawPrompt(st);
    _kb.draw(_curKey);
}

// ---- setup / loop ----------------------------------------------------------
void setup()
{
    M5.begin();
    // Core2 default orientation is 320x240 landscape. M5GFX maps FT6336U touch
    // coordinates into LCD space automatically, so no manual remapping is needed.

    _canvas.setColorDepth(16);
    _canvas.createSprite(kScreenW, kScreenH);
    _botSprite.setColorDepth(16);
    _botSprite.createSprite(kBotW, kBotH);

    _bot.begin(&_botSprite);
    _bot.seedBlink((uint32_t)micros()); // single instance — no decorrelation needed
    _bot.setBattery(100);
    _bot.setSignal(-1); // no radio — hide signal bars

    _settings.begin();
    _settings.apply(&_bot); // theme + brightness

    _kb.begin(&_canvas, &_bot.style(), kKbY);
    _kb.setChipsOnly(_settings.data().kbdLayout == 1);

    _chat.begin(&_canvas, &_bot, &_bot.style(), _clockBuf, _statusBuf);

    _haptics.begin();
    _haptics.setEnabled(_settings.data().haptics);

    _epochMs = millis();
    _timeBaseMs = compileTimeMs();
    _promptLen = 0;
    _prompt[0] = 0;

    _lastTheme = _settings.data().theme;
    _lastBright = _settings.data().brightness;
    _lastHaptics = _settings.data().haptics;
    _lastLayout = _settings.data().kbdLayout;
}

void loop()
{
    _now = millis();
    M5.update();
    handleTouch();

    // long press (nap) while holding the bot still
    if (_down && _zone == ZONE_BOT && !_moved && !_longFired && (_now - _pMs) >= 700)
    {
        _longFired = true;
        _bot.setMood(botux::BotUx::Mood::Sleepy);
        _napUntil = _now + 3000;
        _haptics.cancel();
    }
    if (_napUntil && _now >= _napUntil)
    {
        _napUntil = 0;
        _chat.applyMood(); // restore conversation mood
    }

    // periodic device reads
    if (_now - _lastPowerMs >= 5000)
    {
        _lastPowerMs = _now;
        int8_t batt = (int8_t)M5.Power.getBatteryLevel();
        if (batt < 0 || batt > 100) batt = 100;
        bool charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);
        _bot.setBattery((uint8_t)batt);
        snprintf(_statusBuf, sizeof(_statusBuf), "bat %d%%%s - no signal", batt, charging ? "+" : "");
    }
    updateClock();

    syncSettings();
    _chat.update(_now);
    _bot.update(_now);
    _haptics.update(_now);

    // render
    _bot.draw(); // into _botSprite
    if (_settings.isOpen())
        _settings.draw(&_canvas, &_botSprite);
    else
        drawMain();

    _canvas.pushSprite(0, 0);
    M5.Display.waitDisplay();
}
