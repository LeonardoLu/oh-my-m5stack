#include "BottomLeds.h"

#include <M5Unified.h>
#include <utility/led/LED_Strip_Class.hpp>
#include <memory>

namespace {
constexpr uint8_t kCount = 10;
constexpr int8_t kDataPin = 25;
constexpr uint8_t kBrightness[4] = {0, 16, 38, 72};
constexpr uint8_t kAgentLed[AgentModel::kAgentCount] = {0, 1, 2, 7, 8, 9};

RGBColor colorFor(AgentModel::Status status, uint8_t scale)
{
    struct RGB { uint8_t r, g, b; };
    static const RGB colors[] = {
        {68, 68, 66}, {144, 82, 232}, {48, 116, 238},
        {238, 159, 45}, {47, 180, 104}, {224, 62, 62},
    };
    const RGB& c = colors[static_cast<uint8_t>(status)];
    return RGBColor{
        static_cast<uint8_t>((static_cast<uint16_t>(c.r) * scale) / 255),
        static_cast<uint8_t>((static_cast<uint16_t>(c.g) * scale) / 255),
        static_cast<uint8_t>((static_cast<uint16_t>(c.b) * scale) / 255),
    };
}

uint8_t animationScale(AgentModel::Status status, uint32_t nowMs, bool reduced)
{
    if (reduced || status == AgentModel::Status::Idle || status == AgentModel::Status::Done)
        return 255;
    const uint16_t period = status == AgentModel::Status::Error ? 520 :
                            (status == AgentModel::Status::Waiting ? 1500 : 1050);
    uint16_t phase = nowMs % period;
    uint16_t half = period / 2;
    uint16_t tri = phase < half ? phase : period - phase;
    uint8_t floor = status == AgentModel::Status::Error ? 65 : 105;
    return floor + static_cast<uint8_t>((static_cast<uint32_t>(255 - floor) * tri) / half);
}
}

bool BottomLeds::begin()
{
    auto bus = std::make_shared<m5::LedBus_RMT>();
    auto busConfig = bus->getConfig();
    busConfig.pin_data = kDataPin;
    bus->setConfig(busConfig);

    auto strip = std::make_shared<m5::LED_Strip_Class>();
    auto stripConfig = strip->getConfig();
    stripConfig.led_count = kCount;
    stripConfig.byte_per_led = 3;
    stripConfig.color_order = m5::LED_Strip_Class::config_t::color_order_grb;
    strip->setBus(bus);
    strip->setConfig(stripConfig);
    M5.Led.setLedInstance(strip);
    M5.Led.setAutoDisplay(false);
    _available = M5.Led.begin();
    setBrightness(_brightness);
    if (_available)
    {
        M5.Led.setAllColor(0, 0, 0);
        M5.Led.display();
    }
    return _available;
}

void BottomLeds::setBrightness(uint8_t level)
{
    _brightness = level > 3 ? 2 : level;
    if (_available) M5.Led.setBrightness(kBrightness[_brightness]);
}

void BottomLeds::update(const AgentModel& model, uint32_t nowMs, bool reducedMotion)
{
    if (!_available || nowMs - _lastFrameMs < 50) return;
    _lastFrameMs = nowMs;

    RGBColor colors[kCount]{};
    for (uint8_t i = 0; i < AgentModel::kAgentCount; ++i)
    {
        const AgentModel::Status status = model.agent(i).status;
        const uint8_t scale = animationScale(status, nowMs + i * 73, reducedMotion);
        colors[kAgentLed[i]] = colorFor(status, scale);
    }
    const AgentModel::Status selected = model.selectedAgent().status;
    const uint8_t selectedScale = animationScale(selected, nowMs, reducedMotion);
    const RGBColor selectedColor = colorFor(selected, selectedScale);
    colors[3] = selectedColor;
    colors[4] = selectedColor;
    colors[5] = selectedColor;
    colors[6] = selectedColor;
    M5.Led.setColors(colors, 0, kCount);
    M5.Led.display();
}
