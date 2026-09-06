#include "BottomLedFrame.h"

#include <assert.h>
#include <string.h>

static LightingState sample()
{
    LightingState state;
    for (uint8_t i = 0; i < LightingState::kSlotCount; ++i)
    {
        state.slots[i].color = 0xFF0000;
        state.slots[i].brightness = 255;
        state.slots[i].effect = 1;
    }
    state.ambient.color = 0x008000;
    state.ambient.brightness = 255;
    state.ambient.effect = 1;
    return state;
}

static bool equal(const bottomled::Color (&a)[10], const bottomled::Color (&b)[10])
{
    for (int i = 0; i < 10; ++i)
        if (a[i].r != b[i].r || a[i].g != b[i].g || a[i].b != b[i].b) return false;
    return true;
}

static uint16_t energy(const bottomled::Color& color)
{
    return color.r + color.g + color.b;
}

static void offAndHost()
{
    LightingState lighting = sample();
    bottomled::Events events;
    bottomled::Color colors[10], black[10]{};
    bottomled::frame(lighting, events, 1000, false, true, 0, 0, true, colors);
    assert(equal(colors, black));
    bottomled::frame(lighting, events, 1000, false, true, 0, 2, false, colors);
    assert(equal(colors, black));
    bottomled::frame(lighting, events, 1000, false, false, 0, 1, true, colors);
    assert(equal(colors, black));
    bottomled::frame(lighting, events, 1000, false, true, 0, 1, true, colors);
    assert(colors[0].r == 255 && colors[0].g == 0 && colors[0].b == 0);
    assert(colors[3].g == 128 && colors[3].r == 0 && colors[3].b == 0);
}

static void aliveAndReducedMotion()
{
    LightingState lighting = sample();
    bottomled::Events events;
    bottomled::Color first[10], later[10];
    bottomled::frame(lighting, events, 1000, false, true, 0, 2, true, first);
    bottomled::frame(lighting, events, 3700, false, true, 0, 2, true, later);
    assert(!equal(first, later));
    for (uint8_t i : bottomled::kAgentLed)
        assert(first[i].r > 0 && first[i].g == 0 && first[i].b == 0);
    for (uint8_t i = 3; i <= 6; ++i)
        assert(first[i].g > 0 && first[i].r == 0 && first[i].b == 0);
    bottomled::frame(lighting, events, 1000, true, true, 0, 2, true, first);
    bottomled::frame(lighting, events, 3700, true, true, 0, 2, true, later);
    assert(equal(first, later));
    assert(first[0].r > first[1].r); // selected slot emphasis is static
    lighting.slots[0] = LightingZone{};
    bottomled::frame(lighting, events, 1000, false, true, 0, 2, true, first);
    assert(first[0].r == 0 && first[0].g == 0 && first[0].b == 0);
    bottomled::frame(lighting, events, 1000, false, false, 0, 2, true, first);
    assert(first[0].r > 0 && first[0].g > 0 && first[0].b > 0);
    assert(first[0].r < 80 && first[0].b < 80); // visible but bounded presence
    uint16_t minimum = energy(first[0]), maximum = minimum;
    for (uint8_t i = 1; i < bottomled::kCount; ++i)
    {
        if (energy(first[i]) < minimum) minimum = energy(first[i]);
        if (energy(first[i]) > maximum) maximum = energy(first[i]);
    }
    assert(maximum > minimum); // the cool presence travels across the strip
    bottomled::frame(lighting, events, 3700, false, false, 0, 2, true, later);
    assert(!equal(first, later));
    bottomled::frame(lighting, events, 1000, true, false, 0, 2, true, first);
    bottomled::frame(lighting, events, 3700, true, false, 0, 2, true, later);
    assert(equal(first, later));
}

static void transientFeedbackEnds()
{
    LightingState lighting = sample();
    bottomled::Events events, empty;
    events.hasInteraction = true;
    events.interactionAt = 1000;
    events.interactionSlot = 1;
    events.notificationAt = 1000;
    events.notificationMask = 2;
    bottomled::Color feedback[10], baseline[10];
    bottomled::frame(lighting, events, 1450, false, true, 1, 2, true, feedback);
    bottomled::frame(lighting, empty, 1450, false, true, 1, 2, true, baseline);
    assert(!equal(feedback, baseline));
    assert(feedback[1].r > baseline[1].r);
    bottomled::frame(lighting, events, 3000, false, true, 1, 2, true, feedback);
    bottomled::frame(lighting, empty, 3000, false, true, 1, 2, true, baseline);
    assert(equal(feedback, baseline));
    bottomled::frame(lighting, events, 1450, true, true, 1, 2, true, feedback);
    bottomled::frame(lighting, empty, 1450, true, true, 1, 2, true, baseline);
    assert(equal(feedback, baseline));
    bottomled::frame(lighting, events, 1450, false, true, 1, 1, true, feedback);
    bottomled::frame(lighting, empty, 1450, false, true, 1, 1, true, baseline);
    assert(equal(feedback, baseline)); // Host mode ignores all local events
    events.interactionAt = 0xFFFFFF00u;
    events.notificationAt = 0xFFFFFF00u;
    bottomled::frame(lighting, events, 0x100u, false, true, 1, 2, true, feedback);
    bottomled::frame(lighting, empty, 0x100u, false, true, 1, 2, true, baseline);
    assert(!equal(feedback, baseline)); // duration remains valid across wrap
}

static void controlFeedbackPreservesHostHue()
{
    LightingState lighting = sample();
    bottomled::Events events, empty;
    events.hasControl = true;
    events.controlAt = 1000;
    events.controlColor = 0xF59E32;
    bottomled::Color feedback[10], baseline[10];
    bottomled::frame(lighting, events, 1450, false, true, 0, 2, true, feedback);
    bottomled::frame(lighting, empty, 1450, false, true, 0, 2, true, baseline);
    assert(!equal(feedback, baseline));
    for (uint8_t i : bottomled::kAgentLed)
        assert(feedback[i].r > 0 && feedback[i].g == 0 && feedback[i].b == 0);
    for (uint8_t i = 3; i <= 6; ++i)
        assert(feedback[i].g > 0 && feedback[i].r == 0 && feedback[i].b == 0);

    lighting.ambient = LightingZone{};
    bottomled::frame(lighting, events, 1450, false, true, 0, 2, true, feedback);
    assert(feedback[4].r > feedback[4].g && feedback[4].g > feedback[4].b);
    bottomled::frame(lighting, events, 2000, false, true, 0, 2, true, feedback);
    bottomled::frame(lighting, empty, 2000, false, true, 0, 2, true, baseline);
    assert(equal(feedback, baseline));
    bottomled::frame(lighting, events, 1450, false, true, 0, 1, true, feedback);
    bottomled::frame(lighting, empty, 1450, false, true, 0, 1, true, baseline);
    assert(equal(feedback, baseline));
}

static void heldControlIsKnownAndReducedMotionIsStatic()
{
    LightingState lighting = sample();
    lighting.ambient = LightingZone{};
    bottomled::Events held;
    held.controlHeld = true;
    held.holdColor = 0xF59E32;
    bottomled::Color first[10], later[10];
    bottomled::Events empty;
    bottomled::Color idle[10];
    bottomled::frame(lighting, held, 1000, false, true, 0, 2, true, first);
    bottomled::frame(lighting, empty, 1000, false, true, 0, 2, true, idle);
    bottomled::frame(lighting, held, 1500, false, true, 0, 2, true, later);
    assert(!equal(first, later));
    assert(first[3].r > first[3].g && first[3].g > first[3].b);
    assert(energy(first[3]) > energy(idle[3]));
    bottomled::frame(lighting, held, 1000, true, true, 0, 2, true, first);
    bottomled::frame(lighting, held, 3700, true, true, 0, 2, true, later);
    assert(equal(first, later));
}

static void boundedFrameChanges()
{
    LightingState lighting = sample();
    bottomled::Events events;
    events.hasInteraction = true;
    events.interactionSlot = 2;
    events.notificationMask = 0x3F;
    bottomled::Color previous[10], next[10];
    bottomled::frame(lighting, events, 0, false, true, 2, 2, true, previous);
    for (uint32_t now = 50; now <= 8000; now += 50)
    {
        bottomled::frame(lighting, events, now, false, true, 2, 2, true, next);
        for (int i = 0; i < 10; ++i)
        {
            int delta = static_cast<int>(next[i].r) - previous[i].r;
            assert(delta >= -35 && delta <= 35);
            previous[i] = next[i];
        }
    }
}

int main()
{
    offAndHost();
    aliveAndReducedMotion();
    transientFeedbackEnds();
    controlFeedbackPreservesHostHue();
    heldControlIsKnownAndReducedMotionIsStatic();
    boundedFrameChanges();
}
