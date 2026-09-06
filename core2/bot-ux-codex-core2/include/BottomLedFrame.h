#pragma once

#include <math.h>
#include <stdint.h>

#include "LightingState.h"

namespace bottomled {
constexpr uint8_t kCount = 10;
constexpr uint8_t kAgentLed[LightingState::kSlotCount] = {0, 1, 2, 7, 8, 9};

struct Color {
    uint8_t r, g, b;
};

struct Events {
    uint32_t interactionAt = 0;
    uint32_t notificationAt = 0;
    uint8_t interactionSlot = 0;
    uint8_t notificationMask = 0;
    bool hasInteraction = false;
};

inline uint8_t triangle(uint32_t nowMs, uint16_t period, uint8_t floor)
{
    const uint16_t phase = nowMs % period;
    const uint16_t half = period / 2;
    const uint16_t rising = phase < half ? phase : period - phase;
    return floor + static_cast<uint8_t>((static_cast<uint32_t>(255 - floor) * rising) / half);
}

inline Color renderZone(const LightingZone& zone, uint8_t index, uint32_t nowMs, bool reduced)
{
    if (!zone.active()) return Color{};
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
            return Color{static_cast<uint8_t>((static_cast<uint16_t>(255 - phase * 3) * scale) / 255),
                            static_cast<uint8_t>((static_cast<uint16_t>(phase * 3) * scale) / 255), 0};
        if (phase < 170)
        {
            const uint8_t p = phase - 85;
            return Color{0, static_cast<uint8_t>((static_cast<uint16_t>(255 - p * 3) * scale) / 255),
                            static_cast<uint8_t>((static_cast<uint16_t>(p * 3) * scale) / 255)};
        }
        const uint8_t p = phase - 170;
        return Color{static_cast<uint8_t>((static_cast<uint16_t>(p * 3) * scale) / 255), 0,
                        static_cast<uint8_t>((static_cast<uint16_t>(255 - p * 3) * scale) / 255)};
    }
    const uint8_t r = static_cast<uint8_t>((zone.color >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((zone.color >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(zone.color & 0xFF);
    return Color{static_cast<uint8_t>((static_cast<uint16_t>(r) * scale) / 255),
                    static_cast<uint8_t>((static_cast<uint16_t>(g) * scale) / 255),
                    static_cast<uint8_t>((static_cast<uint16_t>(b) * scale) / 255)};
}
inline Color scaleColor(uint32_t color, uint8_t scale)
{
    return Color{static_cast<uint8_t>(((color >> 16) & 255u) * scale / 255u),
                 static_cast<uint8_t>(((color >> 8) & 255u) * scale / 255u),
                 static_cast<uint8_t>((color & 255u) * scale / 255u)};
}

inline float hump(float phase)
{
    if (phase <= 0.0f || phase >= 1.0f) return 0.0f;
    const float sine = sinf(phase * 3.14159265f);
    return sine * sine;
}

// Pure, allocation-free color generation. Global LED brightness remains in
// the existing strip driver; enabled is false when its user level is zero.
inline void frame(const LightingState& lighting, const Events& events,
                  uint32_t nowMs, bool reduced, bool connected, uint8_t selected,
                  uint8_t mode, bool enabled, Color (&colors)[kCount])
{
    for (uint8_t i = 0; i < kCount; ++i) colors[i] = Color{};
    if (!enabled || mode == 0) return;
    if (mode == 1)
    {
        if (!connected) return;
        for (uint8_t i = 0; i < LightingState::kSlotCount; ++i)
            colors[kAgentLed[i]] = renderZone(lighting.slots[i], i, nowMs, reduced);
        for (uint8_t i = 3; i <= 6; ++i)
            colors[i] = renderZone(lighting.ambient, i, nowMs, reduced);
        return;
    }

    // Slow amplitude-only animation retains the host's exact color ratios.
    const float breath = reduced ? 0.5f :
        0.5f - 0.5f * cosf((nowMs % 5400u) * (6.2831853f / 5400.0f));
    if (!connected)
    {
        // Neutral cool gray communicates controller presence, not agent work.
        const uint8_t level = static_cast<uint8_t>(30.0f + 26.0f * breath);
        for (uint8_t i = 0; i < kCount; ++i) colors[i] = scaleColor(0x84909C, level);
        return;
    }
    if (selected >= LightingState::kSlotCount) selected = 0;
    const uint32_t interactionAge = nowMs - events.interactionAt;
    const bool interacting = !reduced && events.hasInteraction && interactionAge < 900u;
    const float interactionPhase = interactionAge / 900.0f;
    const float sweepHead = interacting ?
        kAgentLed[events.interactionSlot < LightingState::kSlotCount ? events.interactionSlot : 0] +
        6.0f * interactionPhase - 3.0f : 0.0f;
    const uint32_t notificationAge = nowMs - events.notificationAt;
    const float notice = !reduced && notificationAge < 1800u ?
        hump(notificationAge / 1800.0f) : 0.0f;
    for (uint8_t i = 0; i < kCount; ++i)
    {
        int8_t slot = -1;
        for (uint8_t j = 0; j < LightingState::kSlotCount; ++j)
            if (kAgentLed[j] == i) slot = static_cast<int8_t>(j);
        const LightingZone& zone = slot >= 0 ? lighting.slots[slot] : lighting.ambient;
        if (slot >= 0 && !zone.active()) continue;
        // Empty ambient LEDs may show quiet device presence; empty agent LEDs
        // stay black so Alive never invents a per-slot status color.
        const uint32_t color = zone.active() ? zone.color : 0x84909C;
        const uint8_t hostLevel = zone.active() ? zone.brightness : 52;
        float gain = 112.0f + 48.0f * breath;
        if (slot == selected) gain += 30.0f;
        if (!reduced)
        {
            const float angle = (nowMs % 8000u) * (6.2831853f / 8000.0f) - i * 0.62831853f;
            const float wave = 0.5f + 0.5f * cosf(angle);
            gain += 30.0f * wave * wave;
        }
        if (interacting)
        {
            const float distance = fabsf(i - sweepHead);
            if (distance < 2.5f)
                gain += 48.0f * (1.0f - distance / 2.5f) * hump(interactionPhase);
        }
        if (slot >= 0 && (events.notificationMask & (1u << slot))) gain += 80.0f * notice;
        if (gain > 255.0f) gain = 255.0f;
        const uint8_t level = static_cast<uint8_t>(hostLevel * gain / 255.0f);
        colors[i] = scaleColor(color, level);
    }
}

} // namespace bottomled
