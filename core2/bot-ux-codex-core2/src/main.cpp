#include <M5Unified.h>

#include "AgentModel.h"
#include "AudioFeedback.h"
#include "BotUx.h"
#include "BottomLeds.h"
#include "Settings.h"

#include <stdio.h>

namespace {
constexpr int16_t kScreenW = 320;
constexpr int16_t kScreenH = 240;
constexpr int16_t kBotSize = 40;
constexpr uint32_t kFrameMs = 40;

constexpr uint16_t kInk = botux::rgb565(31, 34, 39);
constexpr uint16_t kKey = botux::rgb565(252, 251, 247);
constexpr uint16_t kLine = botux::rgb565(193, 191, 184);
constexpr uint16_t kMuted = botux::rgb565(105, 106, 108);
constexpr uint16_t kWhite = botux::rgb565(255, 255, 255);
constexpr uint16_t kStatusColors[] = {
    botux::rgb565(105, 106, 108), botux::rgb565(135, 82, 222),
    botux::rgb565(48, 112, 224), botux::rgb565(224, 145, 35),
    botux::rgb565(39, 164, 91), botux::rgb565(215, 53, 53),
};

enum class Page : uint8_t { Agents, Control };
enum class TargetType : uint8_t { None, Settings, PageAgents, PageControl, Agent, Command, Workflow, Reasoning };

struct Target {
    Target(TargetType targetType = TargetType::None, int8_t targetIndex = -1)
        : type(targetType), index(targetIndex) {}
    TargetType type;
    int8_t index;
};

M5Canvas canvas(&M5.Display);
M5Canvas botSprite(&M5.Display);
botux::BotUx bot;
AgentModel model;
Settings settings;
AudioFeedback audio;
BottomLeds bottomLeds;

Page page = Page::Agents;
Target pressed;
bool touching = false;
bool screenTouchGesture = false;
bool listening = false;
uint32_t listenStartedMs = 0;
uint32_t nowMs = 0;
uint32_t lastFrameMs = 0;
uint32_t lastPowerMs = 0;
int8_t battery = -1;
bool charging = false;
Settings::Data appliedSettings{};
int16_t lastTouchX = 0;
int16_t lastTouchY = 0;
bool reducedBotActive = false;
uint32_t reducedBotTimeMs = 0;
uint8_t lastBotStatus = 0xFF;
bool lastBotListening = false;

bool sameTarget(const Target& a, const Target& b)
{
    return a.type == b.type && a.index == b.index;
}

Target hitTarget(int16_t x, int16_t y)
{
    if (x < 0 || x >= kScreenW || y < 0 || y >= kScreenH) return {};
    if (y < 40 && x >= 272) return {TargetType::Settings, 0};
    if (y >= 200)
        return {x < 160 ? TargetType::PageAgents : TargetType::PageControl, 0};
    if (page == Page::Agents)
    {
        if (y >= 40 && y < 176)
        {
            const int8_t col = x / 106;
            const int8_t row = (y - 40) / 68;
            const int8_t index = row * 3 + col;
            if (col < 3 && index < AgentModel::kAgentCount) return {TargetType::Agent, index};
        }
    }
    else
    {
        if (y >= 52 && y < 96)
        {
            const int8_t index = x / 80;
            if (index < 2 && model.selectedAgent().status != AgentModel::Status::Waiting) return {};
            return {TargetType::Command, index};
        }
        if (y >= 100 && y < 144) return {TargetType::Workflow, static_cast<int8_t>(x / 80)};
        if (y >= 148 && y < 192)
        {
            const int8_t index = x < 107 ? 0 : (x < 214 ? 1 : 2);
            return {TargetType::Reasoning, index};
        }
    }
    return {};
}

void setBotMood()
{
    if (listening)
    {
        bot.setMood(botux::BotUx::Mood::Listening);
        return;
    }
    switch (model.selectedAgent().status)
    {
        case AgentModel::Status::Thinking: bot.setMood(botux::BotUx::Mood::Thinking); break;
        case AgentModel::Status::Running:  bot.setMood(botux::BotUx::Mood::Working); break;
        case AgentModel::Status::Waiting:  bot.setMood(botux::BotUx::Mood::Waiting); break;
        case AgentModel::Status::Done:     bot.setMood(botux::BotUx::Mood::Done); break;
        case AgentModel::Status::Error:    bot.setMood(botux::BotUx::Mood::Blocked); break;
        default:                           bot.setMood(botux::BotUx::Mood::Idle); break;
    }
}

void syncSettings()
{
    const Settings::Data& data = settings.data();
    if (data.theme != appliedSettings.theme) bot.setStyle(Settings::themeStyle(data.theme));
    if (data.speed != appliedSettings.speed) model.setSpeed(data.speed);
    if (data.audio != appliedSettings.audio) audio.setEnabled(data.audio != 0);
    if (data.ledBrightness != appliedSettings.ledBrightness)
        bottomLeds.setBrightness(data.ledBrightness);
    appliedSettings = data;
}

void activate(const Target& target)
{
    switch (target.type)
    {
        case TargetType::Settings:
            settings.open();
            audio.select();
            break;
        case TargetType::PageAgents:
            page = Page::Agents;
            audio.select();
            break;
        case TargetType::PageControl:
            page = Page::Control;
            audio.select();
            break;
        case TargetType::Agent:
            model.select(target.index);
            audio.select();
            break;
        case TargetType::Command:
            if (target.index == 0) model.accept(nowMs) ? audio.confirm() : audio.error();
            else if (target.index == 1) model.reject() ? audio.reject() : audio.error();
            else if (target.index == 2) { model.submitVoice(nowMs); audio.action(); }
            else { model.startNew(nowMs); audio.action(); }
            break;
        case TargetType::Workflow:
            model.startWorkflow(static_cast<AgentModel::Workflow>(target.index), nowMs);
            audio.action();
            break;
        case TargetType::Reasoning:
            model.setReasoning(static_cast<AgentModel::Reasoning>(target.index));
            audio.select();
            break;
        default:
            break;
    }
    setBotMood();
}

void touchBegin(int16_t x, int16_t y)
{
    touching = true;
    screenTouchGesture = x >= 0 && x < kScreenW && y >= 0 && y < kScreenH;
    lastTouchX = x;
    lastTouchY = y;
    if (settings.isOpen())
    {
        settings.touchBegin(x, y);
        return;
    }
    pressed = hitTarget(x, y);
    if (pressed.type == TargetType::Command && pressed.index == 2)
    {
        listening = true;
        listenStartedMs = nowMs;
        setBotMood();
        audio.select();
    }
}

void touchMove(int16_t x, int16_t y)
{
    if (!touching) return;
    lastTouchX = x;
    lastTouchY = y;
    if (settings.isOpen())
    {
        settings.touchMove(x, y);
        return;
    }
    if (!sameTarget(pressed, hitTarget(x, y)))
    {
        if (listening)
        {
            listening = false;
            setBotMood();
        }
        pressed = {};
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
        return;
    }
    const Target released = hitTarget(x, y);
    const bool valid = sameTarget(pressed, released);
    const bool wasListening = listening;
    listening = false;
    if (valid) activate(pressed);
    else if (wasListening) setBotMood();
    touching = false;
    screenTouchGesture = false;
    pressed = {};
}

bool handleTouch()
{
    const auto detail = M5.Touch.getDetail(0);
    if (detail.wasPressed()) { touchBegin(detail.x, detail.y); return screenTouchGesture; }
    if (detail.isPressed()) { touchMove(detail.x, detail.y); return screenTouchGesture; }
    if (detail.wasReleased())
    {
        const bool screenEvent = screenTouchGesture;
        touchEnd(lastTouchX, lastTouchY);
        return screenEvent;
    }
    return false;
}

void handleButtons(bool screenTouchEvent)
{
    if (screenTouchEvent || touching) return;
    if (settings.isOpen())
    {
        if (M5.BtnB.wasClicked()) { settings.close(); audio.select(); }
        return;
    }
    if (M5.BtnA.wasClicked())
    {
        model.select((model.selected() + AgentModel::kAgentCount - 1) % AgentModel::kAgentCount);
        audio.select();
    }
    else if (M5.BtnB.wasClicked())
    {
        page = page == Page::Agents ? Page::Control : Page::Agents;
        audio.select();
    }
    else if (M5.BtnC.wasClicked())
    {
        model.select((model.selected() + 1) % AgentModel::kAgentCount);
        audio.select();
    }
}

uint16_t foreground() { return settings.data().theme == 2 ? botux::rgb565(239, 239, 235) : kInk; }
uint16_t surface() { return settings.data().theme == 2 ? botux::rgb565(57, 60, 66) : kKey; }

void drawHeader()
{
    const botux::BotUx::Style style = Settings::themeStyle(settings.data().theme);
    botSprite.pushSprite(&canvas, 0, 0);
    canvas.setTextSize(1.0f);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(foreground());
    canvas.drawString("CODEX MICRO", 43, 12);
    canvas.fillRoundRect(43, 22, 30, 14, 5, botux::rgb565(166, 43, 39));
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(kWhite);
    canvas.drawString("SIM", 58, 29);

    char power[12];
    if (battery >= 0) snprintf(power, sizeof(power), "%d%%%s", battery, charging ? "+" : "");
    else snprintf(power, sizeof(power), "--%%");
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(kMuted);
    canvas.drawString(power, 267, 20);

    const bool gearDown = pressed.type == TargetType::Settings;
    canvas.fillRoundRect(272, 2, 46, 36, 8, gearDown ? style.accentColor : surface());
    canvas.drawRoundRect(272, 2, 46, 36, 8, kLine);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(gearDown ? kWhite : foreground());
    canvas.drawString("SET", 295, 20);
}

uint8_t statePulse(AgentModel::Status state, uint8_t index)
{
    if (settings.data().reducedMotion || state == AgentModel::Status::Idle || state == AgentModel::Status::Done)
        return 255;
    const uint16_t period = state == AgentModel::Status::Waiting ? 1500 : 1000;
    const uint16_t phase = (nowMs + index * 79) % period;
    const uint16_t half = period / 2;
    const uint16_t tri = phase < half ? phase : period - phase;
    return 145 + static_cast<uint8_t>((110UL * tri) / half);
}

void drawAgentCard(uint8_t index)
{
    const AgentModel::Agent& agent = model.agent(index);
    const int16_t x = 4 + (index % 3) * 106;
    const int16_t y = 42 + (index / 3) * 68;
    const bool selected = index == model.selected();
    const bool down = pressed.type == TargetType::Agent && pressed.index == index;
    const uint16_t statusColor = kStatusColors[static_cast<uint8_t>(agent.status)];
    canvas.fillRoundRect(x + 2, y + 3, 100, 64, 9, botux::rgb565(179, 177, 170));
    const uint16_t pressedFill = settings.data().theme == 2 ? botux::rgb565(73, 79, 90)
                                                          : botux::rgb565(222, 224, 224);
    canvas.fillRoundRect(x, y, 100, 64, 9, down ? pressedFill : surface());
    canvas.drawRoundRect(x, y, 100, 64, 9, selected ? foreground() : kLine);
    if (selected) canvas.drawRoundRect(x + 2, y + 2, 96, 60, 7, kWhite);

    const uint8_t pulse = statePulse(agent.status, index);
    canvas.fillCircle(x + 13, y + 13, pulse > 215 ? 5 : 4, statusColor);
    char id[3] = {'A', static_cast<char>('1' + index), '\0'};
    canvas.setTextSize(1.0f);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(foreground());
    canvas.drawString(id, x + 23, y + 13);
    canvas.setTextDatum(middle_center);
    canvas.drawString(agent.task, x + 50, y + 33);
    canvas.setTextColor(foreground());
    canvas.drawString(AgentModel::statusName(agent.status), x + 50, y + 51);
}

void drawTabs()
{
    const botux::BotUx::Style style = Settings::themeStyle(settings.data().theme);
    const bool agentDown = pressed.type == TargetType::PageAgents;
    const bool controlDown = pressed.type == TargetType::PageControl;
    canvas.fillRoundRect(4, 201, 154, 37, 8, page == Page::Agents || agentDown ? style.accentColor : surface());
    canvas.fillRoundRect(162, 201, 154, 37, 8, page == Page::Control || controlDown ? style.accentColor : surface());
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(page == Page::Agents || agentDown ? kWhite : foreground());
    canvas.drawString("AGENTS  1/2", 81, 220);
    canvas.setTextColor(page == Page::Control || controlDown ? kWhite : foreground());
    canvas.drawString("CONTROL  2/2", 239, 220);
}

void drawAgentsPage()
{
    for (uint8_t i = 0; i < AgentModel::kAgentCount; ++i) drawAgentCard(i);
    const AgentModel::Agent& active = model.selectedAgent();
    char detail[40];
    snprintf(detail, sizeof(detail), "A%u  %s  /  %s", static_cast<unsigned>(model.selected() + 1), active.task,
             AgentModel::statusName(active.status));
    canvas.setTextSize(1.0f);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(foreground());
    canvas.drawString(detail, 160, 188);
    drawTabs();
}

void drawKey(int16_t x, int16_t y, int16_t w, const char* label, bool active, bool down,
             uint16_t accent, bool enabled = true)
{
    if (!enabled)
    {
        const uint16_t fill = settings.data().theme == 2 ? botux::rgb565(44, 47, 52)
                                                       : botux::rgb565(223, 222, 216);
        const uint16_t labelColor = settings.data().theme == 2 ? botux::rgb565(144, 148, 155)
                                                             : botux::rgb565(111, 114, 119);
        canvas.fillRoundRect(x, y, w, 40, 8, fill);
        canvas.setTextSize(1.0f);
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(labelColor);
        canvas.drawString(label, x + w / 2, y + 20);
        return;
    }
    canvas.fillRoundRect(x + 1, y + 2, w, 40, 8, botux::rgb565(181, 179, 172));
    canvas.fillRoundRect(x, y, w, 40, 8, active || down ? accent : surface());
    canvas.drawRoundRect(x, y, w, 40, 8, kLine);
    canvas.setTextSize(1.0f);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(active || down ? kWhite : foreground());
    canvas.drawString(label, x + w / 2, y + 20);
}

void drawControlPage()
{
    const botux::BotUx::Style style = Settings::themeStyle(settings.data().theme);
    const bool waiting = model.selectedAgent().status == AgentModel::Status::Waiting;
    const char* commands[] = {"ACCEPT", "REJECT", "HOLD MIC", "NEW"};
    for (uint8_t i = 0; i < 4; ++i)
    {
        const bool down = pressed.type == TargetType::Command && pressed.index == i;
        const bool active = (i < 2 && waiting) || (i == 2 && listening);
        const uint16_t accent = i == 1 ? kStatusColors[5] : (i == 0 ? kStatusColors[4] : style.accentColor);
        drawKey(i * 80 + 2, 54, 76, commands[i], active, down, accent, i >= 2 || waiting);
    }

    const char* workflows[] = {"^ REVIEW", "< DEBUG", "REFACTOR >", "v NEXT"};
    for (uint8_t i = 0; i < 4; ++i)
    {
        const bool down = pressed.type == TargetType::Workflow && pressed.index == i;
        drawKey(i * 80 + 2, 102, 76, workflows[i], false, down, style.accentColor);
    }

    const char* reasoning[] = {"LOW", "MEDIUM", "HIGH"};
    const int16_t starts[] = {2, 109, 216};
    for (uint8_t i = 0; i < 3; ++i)
    {
        const bool down = pressed.type == TargetType::Reasoning && pressed.index == i;
        const bool active = static_cast<uint8_t>(model.reasoning()) == i;
        drawKey(starts[i], 150, 102, reasoning[i], active, down, style.accentColor);
    }

    char detail[40];
    if (listening)
        snprintf(detail, sizeof(detail), "LISTENING  %lus",
                 static_cast<unsigned long>((nowMs - listenStartedMs) / 1000));
    else
        snprintf(detail, sizeof(detail), "A%u %s  -  REASONING %s", static_cast<unsigned>(model.selected() + 1),
                 AgentModel::statusName(model.selectedAgent().status), AgentModel::reasoningName(model.reasoning()));
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(listening ? style.accentColor : foreground());
    canvas.drawString(detail, 160, 46);
    drawTabs();
}

void readPower()
{
    const int32_t level = M5.Power.getBatteryLevel();
    battery = level >= 0 && level <= 100 ? static_cast<int8_t>(level) : -1;
    charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
}

void failSprite(const char* message)
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_RED);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(message, 160, 120);
    while (true) delay(1000);
}
}

void setup()
{
    auto config = M5.config();
    config.clear_display = true;
    config.internal_spk = true;
    M5.begin(config);
    M5.Display.setRotation(1);

    canvas.setColorDepth(16);
    if (!canvas.createSprite(kScreenW, kScreenH)) failSprite("DISPLAY BUFFER FAILED");
    botSprite.setColorDepth(16);
    if (!botSprite.createSprite(kBotSize, kBotSize)) failSprite("BOT BUFFER FAILED");

    settings.begin();
    bot.begin(&botSprite);
    bot.seedBlink(static_cast<uint32_t>(micros()));
    bot.setBatteryVisible(false);
    bot.setSignal(-1);
    settings.apply(bot);
    model.begin(millis());
    model.setSpeed(settings.data().speed);
    audio.setEnabled(settings.data().audio != 0);
    bottomLeds.begin();
    bottomLeds.setBrightness(settings.data().ledBrightness);
    appliedSettings = settings.data();
    readPower();
    setBotMood();
}

void loop()
{
    nowMs = millis();
    M5.update();
    const bool screenTouchEvent = handleTouch();
    handleButtons(screenTouchEvent);
    if (nowMs - lastFrameMs < kFrameMs)
    {
        delay(1);
        return;
    }
    lastFrameMs = nowMs;
    if (nowMs - lastPowerMs >= 5000)
    {
        lastPowerMs = nowMs;
        readPower();
    }
    syncSettings();
    model.update(nowMs);
    setBotMood();
    const bool reduced = settings.data().reducedMotion != 0;
    const uint8_t status = static_cast<uint8_t>(model.selectedAgent().status);
    const bool poseChanged = status != lastBotStatus || listening != lastBotListening;
    if (!reduced)
    {
        bot.update(nowMs);
        reducedBotActive = false;
    }
    else if (!reducedBotActive)
    {
        reducedBotTimeMs = nowMs;
        bot.update(reducedBotTimeMs);
        reducedBotActive = true;
    }
    else if (poseChanged)
    {
        // Let the new pose settle in bounded synthetic steps, then freeze it.
        if (nowMs - reducedBotTimeMs >= 500)
            for (uint8_t i = 0; i < 6; ++i) bot.update(nowMs - 500 + i * 100);
        else
            bot.update(nowMs);
        reducedBotTimeMs = nowMs;
    }
    else
    {
        bot.update(reducedBotTimeMs);
    }
    lastBotStatus = status;
    lastBotListening = listening;
    bot.draw();
    bottomLeds.update(model, nowMs, reduced);

    canvas.fillSprite(Settings::themeStyle(settings.data().theme).bgColor);
    if (settings.isOpen()) settings.draw(canvas, botSprite);
    else
    {
        drawHeader();
        if (page == Page::Agents) drawAgentsPage();
        else drawControlPage();
    }
    canvas.pushSprite(0, 0);
}
