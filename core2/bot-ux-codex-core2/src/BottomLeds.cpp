#include "BottomLeds.h"

#include <M5Unified.h>
#include <utility/led/LED_Strip_Class.hpp>
#include <memory>

namespace {
constexpr uint8_t kCount = 10;
constexpr int8_t kDataPin = 25;
constexpr uint8_t kBrightness[4] = {0, 16, 38, 72};
constexpr uint8_t kAgentLed[LightingState::kSlotCount] = {0, 1, 2, 7, 8, 9};

uint8_t triangle(uint32_t nowMs, uint16_t period, uint8_t floor)
{
    const uint16_t phase = nowMs % period;
    const uint16_t half = period / 2;
    const uint16_t rising = phase < half ? phase : period - phase;
    return floor + static_cast<uint8_t>((static_cast<uint32_t>(255 - floor) * rising) / half);
}

RGBColor renderZone(const LightingZone& zone, uint8_t index, uint32_t nowMs, bool reduced)
{
    if (!zone.active()) return RGBColor{};
    uint8_t scale = zone.brightness;
    const uint16_t period = 1800 - static_cast<uint16_t>(zone.speed) * 5;
    if (!reduced && (zone.effect == 4 || zone.effect == 6))
        scale = static_cast<uint8_t>((static_cast<uint16_t>(scale) *
                 triangle(nowMs, period < 400 ? 400 : period, zone.effect == 6 ? 170 : 70)) / 255);
    if (!reduced && zone.effect == 2)
    {
        const uint8_t head = static_cast<uint8_t>((nowMs / (40 + (255 - zone.speed) / 4)) % kCount);
        if (index != head) scale = static_cast<uint8_t>(scale / 7);
    }
    if (!reduced && zone.effect == 3)
    {
        const uint8_t phase = static_cast<uint8_t>(nowMs / 15 + index * 23);
        if (phase < 85)
            return RGBColor{static_cast<uint8_t>((static_cast<uint16_t>(255 - phase * 3) * scale) / 255),
                            static_cast<uint8_t>((static_cast<uint16_t>(phase * 3) * scale) / 255), 0};
        if (phase < 170)
        {
            const uint8_t p = phase - 85;
            return RGBColor{0, static_cast<uint8_t>((static_cast<uint16_t>(255 - p * 3) * scale) / 255),
                            static_cast<uint8_t>((static_cast<uint16_t>(p * 3) * scale) / 255)};
        }
        const uint8_t p = phase - 170;
        return RGBColor{static_cast<uint8_t>((static_cast<uint16_t>(p * 3) * scale) / 255), 0,
                        static_cast<uint8_t>((static_cast<uint16_t>(255 - p * 3) * scale) / 255)};
    }
    const uint8_t r = static_cast<uint8_t>((zone.color >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((zone.color >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(zone.color & 0xFF);
    return RGBColor{static_cast<uint8_t>((static_cast<uint16_t>(r) * scale) / 255),
                    static_cast<uint8_t>((static_cast<uint16_t>(g) * scale) / 255),
                    static_cast<uint8_t>((static_cast<uint16_t>(b) * scale) / 255)};
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

void BottomLeds::update(const LightingState& lighting, uint32_t nowMs, bool reducedMotion,
                        bool connected)
{
    if (!_available || nowMs - _lastFrameMs < 50) return;
    _lastFrameMs = nowMs;

    RGBColor colors[kCount]{};
    if (connected)
    {
        for (uint8_t i = 0; i < LightingState::kSlotCount; ++i)
            colors[kAgentLed[i]] = renderZone(lighting.slots[i], i, nowMs, reducedMotion);
        for (uint8_t i = 3; i <= 6; ++i)
            colors[i] = renderZone(lighting.ambient, i, nowMs, reducedMotion);
    }
    M5.Led.setColors(colors, 0, kCount);
    M5.Led.display();
}
