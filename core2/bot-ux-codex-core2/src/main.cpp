#include <M5Unified.h>

#include "AudioFeedback.h"
#include "AnalogInput.h"
#include "BatteryDoubleTap.h"
#include "BotUx.h"
#include "BottomLeds.h"
#include "CodexLink.h"
#include "Settings.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr int16_t kScreenW = 320;
constexpr int16_t kScreenH = 240;
constexpr int16_t kBotSize = 40;
constexpr uint32_t kFrameMs = 33;
constexpr uint16_t kInk = botux::rgb565(31, 34, 39);
constexpr uint16_t kKey = botux::rgb565(252, 251, 247);
constexpr uint16_t kLine = botux::rgb565(193, 191, 184);
constexpr uint16_t kMuted = botux::rgb565(105, 106, 108);
constexpr uint16_t kWhite = botux::rgb565(255, 255, 255);
constexpr uint16_t kGreen = botux::rgb565(39, 164, 91);
constexpr uint16_t kRed = botux::rgb565(215, 53, 53);

enum class Page : uint8_t { Agents, Control };
enum class TargetType : uint8_t {
    None, Battery, PageAgents, PageControl, Agent, Command, Reasoning, Joystick
};
struct Target {
    Target(TargetType targetType = TargetType::None, int8_t targetIndex = -1)
        : type(targetType), index(targetIndex) {}
    TargetType type;
    int8_t index;
};

M5Canvas canvas(&M5.Display);
M5Canvas botSprite(&M5.Display);
botux::BotUx bot;
Settings settings;
AudioFeedback audio;
BottomLeds bottomLeds;
CodexLink codexLink;
BatteryDoubleTap batteryDoubleTap;
Page page = Page::Agents;
Target pressed;
bool touching = false;
bool screenTouchGesture = false;
bool listening = false;
bool pttSent = false;
bool uiDirty = true;
uint8_t selectedAgent = 0;
uint32_t nowMs = 0;
uint32_t lastFrameMs = 0;
uint32_t lastPowerMs = 0;
int8_t battery = -1;
bool charging = false;
Settings::Data appliedSettings{};
CodexLink::State lastLinkState = CodexLink::State::Starting;
bool lastControlReady = false;
uint32_t lastLightingRevision = 0;
float lastAx = 0.0f;
float lastAy = 0.0f;
float lastAz = 1.0f;
uint32_t statsStartedMs = 0;
uint32_t frameCount = 0;
uint32_t fullPushCount = 0;
uint32_t drawMaxUs = 0;
uint32_t pushMaxUs = 0;
char serialCommand[16]{};
uint8_t serialLength = 0;
bool joystickMoved = false;
bool joystickActive = false;
float joystickAngle = 0.0f;
uint32_t lastJoystickMs = 0;
bool captureRequested = false;

bool sameTarget(const Target& a, const Target& b)
{
    return a.type == b.type && a.index == b.index;
}

Target hitTarget(int16_t x, int16_t y)
{
    if (x < 0 || x >= kScreenW || y < 0 || y >= kScreenH) return {};
    if (y < 40 && x >= 224) return {TargetType::Battery, 0};
    if (y >= 200) return {x < 160 ? TargetType::PageAgents : TargetType::PageControl, 0};
    if (page == Page::Agents && y >= 44 && y < 174)
    {
        const int8_t col = x / 106;
        const int8_t row = (y - 44) / 65;
        const int8_t index = row * 3 + col;
        if (col < 3 && index < LightingState::kSlotCount) return {TargetType::Agent, index};
    }
    if (page == Page::Control)
    {
        if ((y >= 48 && y < 88) || (y >= 92 && y < 132))
        {
            const int8_t row = y >= 92 ? 1 : 0;
            return {TargetType::Command, static_cast<int8_t>(row * 3 + x / 106)};
        }
        if (y >= 140 && y < 190)
        {
            if (x < 64) return {TargetType::Reasoning, 0};
            if (x >= 256) return {TargetType::Reasoning, 1};
            return {TargetType::Joystick, 0};
        }
    }
    return {};
}

uint16_t foreground() { return settings.data().theme == 2 ? botux::rgb565(239, 239, 235) : kInk; }
uint16_t surface() { return settings.data().theme == 2 ? botux::rgb565(57, 60, 66) : kKey; }
uint16_t mutedText()
{
    return settings.data().theme == 2 ? botux::rgb565(184, 187, 192) : kMuted;
}
uint16_t rgb24to565(uint32_t color)
{
    return botux::rgb565(static_cast<uint8_t>(color >> 16),
                         static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color));
}

void setBotMood()
{
    if (listening) bot.setMood(botux::BotUx::Mood::Listening);
    else if (codexLink.controlReady()) bot.setMood(botux::BotUx::Mood::Idle);
    else if (codexLink.connected()) bot.setMood(botux::BotUx::Mood::Waiting);
    else bot.setMood(botux::BotUx::Mood::Sleepy);
}

void syncSettings()
{
    const Settings::Data& data = settings.data();
    if (memcmp(&data, &appliedSettings, sizeof(data)) == 0) return;
    settings.apply(bot);
    audio.setEnabled(data.audio != 0);
    bottomLeds.setBrightness(data.ledBrightness);
    appliedSettings = data;
    uiDirty = true;
}

bool sendTap(const char* key)
{
    if (!codexLink.tapKey(key, selectedAgent)) { audio.error(); return false; }
    audio.action();
    bot.poke();
    return true;
}

void activate(const Target& target)
{
    static const char* actionKeys[] = {"ACT06", "ACT07", "ACT08", "ACT09", "ACT10", "ACT12"};
    switch (target.type)
    {
        case TargetType::Battery:
            if (batteryDoubleTap.tap(nowMs)) { settings.open(); audio.select(); }
            break;
        case TargetType::PageAgents: page = Page::Agents; audio.select(); break;
        case TargetType::PageControl: page = Page::Control; audio.select(); break;
        case TargetType::Agent:
        {
            selectedAgent = static_cast<uint8_t>(target.index);
            char key[] = "AG00";
            key[3] = static_cast<char>('0' + selectedAgent);
            sendTap(key);
            break;
        }
        case TargetType::Command:
            if (target.index == 4) { if (pttSent) audio.action(); }
            else if (target.index >= 0 && target.index < 6) sendTap(actionKeys[target.index]);
            break;
        case TargetType::Reasoning:
            if (target.index >= 0)
            {
                if (codexLink.turnEncoder(target.index == 0)) { audio.select(); bot.poke(); }
                else audio.error();
            }
            break;
        case TargetType::Joystick:
            if (codexLink.tapKey("ENC_PRESS", selectedAgent)) { audio.select(); bot.poke(); }
            else audio.error();
            break;
        default: break;
    }
    setBotMood();
    uiDirty = true;
}

void finishPtt()
{
    if (pttSent) codexLink.releaseKey("ACT10", selectedAgent);
    pttSent = false;
    listening = false;
    setBotMood();
}

void touchBegin(int16_t x, int16_t y)
{
    touching = true;
    screenTouchGesture = x >= 0 && x < kScreenW && y >= 0 && y < kScreenH;
    if (settings.isOpen())
    {
        batteryDoubleTap.cancel();
        settings.touchBegin(x, y);
        syncSettings();
        uiDirty = true;
        return;
    }
    pressed = hitTarget(x, y);
    if (pressed.type != TargetType::Battery) batteryDoubleTap.cancel();
    joystickMoved = false;
    joystickActive = false;
    if (pressed.type == TargetType::Command && pressed.index == 4)
    {
        pttSent = codexLink.pressKey("ACT10", selectedAgent);
        listening = pttSent;
        if (pttSent) audio.select(); else audio.error();
        setBotMood();
    }
    uiDirty = true;
}

void touchMove(int16_t x, int16_t y)
{
    if (!touching) return;
    if (settings.isOpen())
    {
        if (settings.touchMove(x, y))
        {
            syncSettings();
            uiDirty = true;
        }
        return;
    }
    if (pressed.type == TargetType::Joystick)
    {
        const float dx = static_cast<float>(x - 160);
        const float dy = static_cast<float>(y - 163);
        const AnalogPosition position = AnalogInput::fromOffset(dx, dy, 45.0f, 7.0f);
        if (position.active)
        {
            joystickAngle = position.angle;
            if (!joystickMoved || nowMs - lastJoystickMs >= 45)
            {
                if (codexLink.moveJoystick(joystickAngle, position.distance))
                    joystickActive = true;
                lastJoystickMs = nowMs;
            }
            joystickMoved = true;
        }
        else if (joystickActive)
        {
            codexLink.moveJoystick(joystickAngle, 0);
            joystickActive = false;
        }
        return;
    }
    if (pressed.type != TargetType::None && !sameTarget(pressed, hitTarget(x, y)))
    {
        if (pttSent) finishPtt();
        if (pressed.type == TargetType::Battery) batteryDoubleTap.cancel();
        pressed = {};
        uiDirty = true;
    }
}

void touchEnd(int16_t x, int16_t y)
{
    if (!touching) return;
    if (settings.isOpen())
    {
        settings.touchEnd(x, y);
        touching = false;
        screenTouchGesture = false;
        syncSettings();
        uiDirty = true;
        return;
    }
    const Target released = hitTarget(x, y);
    const bool valid = sameTarget(pressed, released);
    const bool wasPtt = pressed.type == TargetType::Command && pressed.index == 4;
    const bool wasJoystick = pressed.type == TargetType::Joystick;
    if (pttSent) finishPtt();
    if (wasJoystick && joystickActive)
    {
        codexLink.moveJoystick(joystickAngle, 0);
        joystickActive = false;
    }
    if (valid)
    {
        if (wasPtt) { audio.action(); bot.poke(); }
        else if (wasJoystick && joystickMoved) audio.select();
        else activate(pressed);
    }
    touching = false;
    screenTouchGesture = false;
    pressed = {};
    uiDirty = true;
}

bool handleTouch()
{
    const auto detail = M5.Touch.getDetail(0);
    if (detail.wasPressed()) { touchBegin(detail.x, detail.y); return screenTouchGesture; }
    if (detail.isPressed()) { touchMove(detail.x, detail.y); return screenTouchGesture; }
    if (detail.wasReleased())
    {
        const bool screenEvent = screenTouchGesture;
        touchEnd(detail.x, detail.y);
        return screenEvent;
    }
    return false;
}

void handleButtons(bool screenTouchEvent)
{
    if (screenTouchEvent || touching) return;
    if (settings.isOpen())
    {
        if (M5.BtnB.wasClicked())
        {
            batteryDoubleTap.cancel();
            settings.close();
            audio.select();
            uiDirty = true;
        }
        return;
    }
    if (M5.BtnA.wasClicked())
    {
        batteryDoubleTap.cancel();
        selectedAgent = (selectedAgent + 5) % 6;
        audio.select();
        uiDirty = true;
    }
    else if (M5.BtnB.wasClicked())
    {
        batteryDoubleTap.cancel();
        page = page == Page::Agents ? Page::Control : Page::Agents;
        audio.select();
        uiDirty = true;
    }
    else if (M5.BtnC.wasClicked())
    {
        batteryDoubleTap.cancel();
        selectedAgent = (selectedAgent + 1) % 6;
        audio.select();
        uiDirty = true;
    }
}

void drawHeader()
{
    const botux::BotUx::Style style = Settings::themeStyle(settings.data().theme);
    botSprite.pushSprite(&canvas, 0, 0);
    const bool ble = codexLink.connected();
    const bool ready = codexLink.controlReady();
    const uint16_t bleColor = ble ? style.accentColor : mutedText();
    canvas.fillRoundRect(47, 5, 30, 30, 8, ble ? bleColor : surface());
    if (!ble) canvas.drawRoundRect(47, 5, 30, 30, 8, kLine);
    const uint16_t bleInk = ble ? kWhite : bleColor;
    canvas.drawFastVLine(62, 10, 20, bleInk);
    canvas.drawLine(62, 10, 70, 17, bleInk);
    canvas.drawLine(70, 17, 55, 27, bleInk);
    canvas.drawLine(62, 30, 70, 23, bleInk);
    canvas.drawLine(70, 23, 55, 13, bleInk);

    canvas.fillRoundRect(84, 7, 32, 25, 6, ready ? kGreen : surface());
    canvas.drawRoundRect(84, 7, 32, 25, 6, ready ? kGreen : kLine);
    const uint16_t appInk = ready ? kWhite : mutedText();
    canvas.drawRoundRect(91, 12, 18, 12, 3, appInk);
    canvas.drawFastVLine(100, 24, 4, appInk);
    canvas.drawFastHLine(95, 28, 10, appInk);

    const bool down = pressed.type == TargetType::Battery;
    if (down) canvas.fillRoundRect(224, 4, 92, 32, 8, surface());
    char power[5];
    if (battery >= 0) snprintf(power, sizeof(power), "%d", battery);
    else snprintf(power, sizeof(power), "--");
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(down ? foreground() : mutedText());
    canvas.drawString(power, 272, 20);
    const uint16_t powerColor = charging ? style.accentColor : foreground();
    canvas.drawRoundRect(279, 12, 28, 16, 3, powerColor);
    canvas.fillRect(307, 17, 3, 6, powerColor);
    if (battery > 0)
    {
        const int16_t width = static_cast<int16_t>((static_cast<int32_t>(battery) * 22) / 100);
        canvas.fillRect(282, 15, width, 10, powerColor);
    }
    if (charging)
    {
        canvas.drawLine(293, 13, 289, 21, kWhite);
        canvas.drawLine(289, 21, 295, 19, kWhite);
        canvas.drawLine(295, 19, 292, 27, kWhite);
    }
}

void drawAgentCard(uint8_t index)
{
    const int16_t x = 4 + (index % 3) * 106;
    const int16_t y = 44 + (index / 3) * 65;
    const bool selected = index == selectedAgent;
    const bool down = pressed.type == TargetType::Agent && pressed.index == index;
    const LightingZone& zone = codexLink.lighting().slots[index];
    const uint16_t color = zone.active() ? rgb24to565(zone.color) : mutedText();
    const uint16_t fill = down ? (settings.data().theme == 2 ? botux::rgb565(78, 82, 91)
                                                            : botux::rgb565(220, 225, 231)) : surface();
    canvas.fillRoundRect(x, y, 100, 60, 9, fill);
    canvas.drawRoundRect(x, y, 100, 60, 9,
                         selected ? Settings::themeStyle(settings.data().theme).accentColor : kLine);
    if (selected) canvas.drawRoundRect(x + 2, y + 2, 96, 56, 7, foreground());
    canvas.fillCircle(x + 16, y + 18, 6, color);
    char title[12];
    snprintf(title, sizeof(title), "AGENT %u", static_cast<unsigned>(index + 1));
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(foreground());
    canvas.drawString(title, x + 28, y + 18);
    canvas.setTextColor(mutedText());
    canvas.drawString(zone.active() ? "HOST COLOR" : "NO COLOR", x + 12, y + 43);
}

void drawTabs()
{
    const uint16_t accent = Settings::themeStyle(settings.data().theme).accentColor;
    if (pressed.type == TargetType::PageAgents) canvas.fillRect(0, 201, 160, 39, surface());
    if (pressed.type == TargetType::PageControl) canvas.fillRect(160, 201, 160, 39, surface());
    canvas.drawFastHLine(0, 200, 320, accent);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(foreground());
    canvas.drawString("<", 34, 220);
    canvas.drawString(page == Page::Agents ? "1 / 2" : "2 / 2", 160, 220);
    canvas.drawString(">", 286, 220);
}

void drawAgentsPage()
{
    for (uint8_t i = 0; i < LightingState::kSlotCount; ++i) drawAgentCard(i);
    char detail[16];
    snprintf(detail, sizeof(detail), "SLOT %u / 6", static_cast<unsigned>(selectedAgent + 1));
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(mutedText());
    canvas.drawString(detail, 160, 183);
    drawTabs();
}

void drawKey(int16_t x, int16_t y, int16_t w, const char* label, const char* code,
             bool down, uint16_t accent)
{
    canvas.fillRoundRect(x, y, w, 40, 8, down ? accent : surface());
    canvas.drawRoundRect(x, y, w, 40, 8, kLine);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(down ? kWhite : foreground());
    canvas.drawString(label, x + w / 2, y + 12);
    canvas.setTextColor(down ? kWhite : mutedText());
    canvas.drawString(code, x + w / 2, y + 29);
}

void drawControlPage()
{
    static const char* labels[] = {"FAST", "APPROVE", "DECLINE", "FORK", "HOLD MIC", "SEND"};
    static const char* hints[] = {"TOGGLE", "REQUEST", "REQUEST", "TASK", "MAC AUDIO", "COMPOSER"};
    const uint16_t styleAccent = Settings::themeStyle(settings.data().theme).accentColor;
    const uint16_t keyAccent = codexLink.lighting().keys.active()
                                   ? rgb24to565(codexLink.lighting().keys.color) : styleAccent;
    for (uint8_t i = 0; i < 6; ++i)
    {
        const int16_t x = 4 + (i % 3) * 106;
        const int16_t y = i < 3 ? 48 : 92;
        const bool down = pressed.type == TargetType::Command && pressed.index == i;
        drawKey(x, y, 100, labels[i], hints[i], down || (i == 4 && listening),
                i == 2 ? kRed : (i == 1 ? kGreen : keyAccent));
    }
    const bool lessDown = pressed.type == TargetType::Reasoning && pressed.index == 0;
    const bool moreDown = pressed.type == TargetType::Reasoning && pressed.index == 1;
    drawKey(4, 143, 58, "LESS", "TURN", lessDown, styleAccent);
    canvas.fillRoundRect(66, 143, 188, 40, 8, surface());
    canvas.drawRoundRect(66, 143, 188, 40, 8, kLine);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(foreground());
    canvas.drawString("TAP KNOB / DRAG STICK", 160, 163);
    drawKey(258, 143, 58, "MORE", "TURN", moreDown, styleAccent);
    drawTabs();
}

void readPower()
{
    battery = M5.Power.getBatteryLevel();
    charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
}

void updateMotion()
{
    float ax = 0.0f, ay = 0.0f, az = 1.0f;
    if (!M5.Imu.getAccel(&ax, &ay, &az)) return;
    const float delta = fabsf(ax - lastAx) + fabsf(ay - lastAy) + fabsf(az - lastAz);
    float shake = (delta - 0.12f) * 2.5f;
    if (shake < 0.0f) shake = 0.0f;
    if (shake > 1.0f) shake = 1.0f;
    bot.setMotion(ay, -ax, shake);
    lastAx = ax; lastAy = ay; lastAz = az;
}

void handleSerial()
{
    while (Serial.available())
    {
        const char value = static_cast<char>(Serial.read());
        if (value == '\n' || value == '\r')
        {
            if (serialLength)
            {
                serialCommand[serialLength] = '\0';
                bool sent = false;
                if (strcmp(serialCommand, "next") == 0) sent = codexLink.turnEncoder(false);
                else if (strcmp(serialCommand, "prev") == 0) sent = codexLink.turnEncoder(true);
                else if (strcmp(serialCommand, "c") == 0)
                {
                    captureRequested = true;
                    uiDirty = true;
                    serialLength = 0;
                    return;
                }
                else if (serialLength == 1 && serialCommand[0] >= '0' && serialCommand[0] <= '5')
                {
                    if (serialCommand[0] == '0') { settings.close(); page = Page::Agents; }
                    else if (serialCommand[0] == '1') { settings.close(); page = Page::Control; }
                    else settings.open(static_cast<uint8_t>(serialCommand[0] - '2'));
                    uiDirty = true;
                    serialLength = 0;
                    return;
                }
                Serial.printf("[hid] command=%s sent=%s ready=%u\n", serialCommand,
                              sent ? "yes" : "no", codexLink.controlReady());
                serialLength = 0;
            }
        }
        else if (serialLength + 1 < sizeof(serialCommand)) serialCommand[serialLength++] = value;
        else serialLength = 0;
    }
}

void writeFrameCapture()
{
    const uint8_t* pixels = static_cast<const uint8_t*>(canvas.getBuffer());
    Serial.printf("FRAME 320 240 RGB565BE 153600\n");
    for (size_t offset = 0; offset < 320u * 240u * 2u; offset += 2048)
    {
        const size_t remaining = 320u * 240u * 2u - offset;
        Serial.write(pixels + offset, remaining < 2048 ? remaining : 2048);
    }
}

void failSprite(const char* message)
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_RED);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(message, 160, 120);
    while (true) delay(1000);
}

void reportPerformance()
{
    const uint32_t elapsed = nowMs - statsStartedMs;
    if (elapsed < 5000) return;
    const float fps = elapsed ? frameCount * 1000.0f / elapsed : 0.0f;
    Serial.printf("[perf] fps=%.2f draw_max_us=%lu push_max_us=%lu full=%lu frames=%lu "
                  "heap=%u psram=%u ble=%u ready=%u mtu=%u rpc=%lu events=%lu\n",
                  fps, static_cast<unsigned long>(drawMaxUs), static_cast<unsigned long>(pushMaxUs),
                  static_cast<unsigned long>(fullPushCount), static_cast<unsigned long>(frameCount),
                  ESP.getFreeHeap(), ESP.getFreePsram(), codexLink.connected(), codexLink.controlReady(),
                  codexLink.peerMtu(), static_cast<unsigned long>(codexLink.receivedRpcCount()),
                  static_cast<unsigned long>(codexLink.sentEventCount()));
    statsStartedMs = nowMs;
    frameCount = fullPushCount = drawMaxUs = pushMaxUs = 0;
}
}

void setup()
{
    Serial.begin(115200);
    auto config = M5.config();
    config.clear_display = true;
    config.internal_spk = true;
    config.internal_imu = true;
    M5.begin(config);
    M5.Display.setRotation(1);
    canvas.setColorDepth(16);
    if (!canvas.createSprite(kScreenW, kScreenH)) failSprite("DISPLAY BUFFER FAILED");
    botSprite.setColorDepth(16);
    if (!botSprite.createSprite(kBotSize, kBotSize)) failSprite("BOT BUFFER FAILED");
    canvas.setFont(&fonts::Font2);
    settings.begin();
    bot.begin(&botSprite);
    bot.seedBlink(static_cast<uint32_t>(micros()));
    bot.setBatteryVisible(false);
    bot.setSignal(-1);
    settings.apply(bot);
    audio.setEnabled(settings.data().audio != 0);
    bottomLeds.begin();
    bottomLeds.setBrightness(settings.data().ledBrightness);
    appliedSettings = settings.data();
    readPower();
    const bool started = codexLink.begin();
    Serial.printf("[hid] start=%s local_mtu=67 name=Core2 Codex Micro\n",
                  started ? "ok" : "failed");
    setBotMood();
    statsStartedMs = millis();
}

void loop()
{
    nowMs = millis();
    M5.update();
    handleSerial();
    codexLink.update(battery, charging, nowMs);
    const bool screenTouchEvent = handleTouch();
    handleButtons(screenTouchEvent);
    if (codexLink.state() != lastLinkState || codexLink.controlReady() != lastControlReady)
    {
        lastLinkState = codexLink.state();
        lastControlReady = codexLink.controlReady();
        setBotMood();
        uiDirty = true;
        Serial.printf("[hid] ble=%u ready=%u mtu=%u\n", codexLink.connected(), codexLink.controlReady(), codexLink.peerMtu());
    }
    if (codexLink.lightingRevision() != lastLightingRevision)
    {
        lastLightingRevision = codexLink.lightingRevision();
        uiDirty = true;
    }
    if (nowMs - lastFrameMs < kFrameMs) { delay(1); return; }
    lastFrameMs = nowMs;
    if (nowMs - lastPowerMs >= 5000) { lastPowerMs = nowMs; readPower(); }
    syncSettings();
    updateMotion();
    bot.update(nowMs);
    const uint32_t drawStarted = micros();
    bot.draw();
    bottomLeds.update(codexLink.lighting(), nowMs, settings.data().reducedMotion != 0,
                      codexLink.controlReady());
    const bool fullFrame = uiDirty;
    if (fullFrame)
    {
        canvas.fillSprite(Settings::themeStyle(settings.data().theme).bgColor);
        if (settings.isOpen()) settings.draw(canvas, botSprite);
        else { drawHeader(); if (page == Page::Agents) drawAgentsPage(); else drawControlPage(); }
    }
    else if (!settings.isOpen())
    {
        canvas.fillRect(0, 0, kScreenW, 40, Settings::themeStyle(settings.data().theme).bgColor);
        drawHeader();
    }
    const uint32_t drawUs = micros() - drawStarted;
    if (drawUs > drawMaxUs) drawMaxUs = drawUs;
    const uint32_t pushStarted = micros();
    if (fullFrame)
    {
        canvas.pushSprite(0, 0);
        ++fullPushCount;
        uiDirty = false;
    }
    else if (settings.isOpen()) botSprite.pushSprite(2, 0);
    else
    {
        M5.Display.pushImage(0, 0, kScreenW, 40, static_cast<uint16_t*>(canvas.getBuffer()));
    }
    const uint32_t pushUs = micros() - pushStarted;
    if (pushUs > pushMaxUs) pushMaxUs = pushUs;
    ++frameCount;
    if (captureRequested)
    {
        captureRequested = false;
        writeFrameCapture();
    }
    reportPerformance();
}
