#include "WatchInteraction.h"
#include "InputSemantics.h"

#include <assert.h>

using namespace watchinteraction;

static void testHsvKeyColorsAndSectorBoundary() {
    auto red = hsvRgb(0, 100, 100);
    auto yellowEdge = hsvRgb(59, 100, 100);
    auto yellow = hsvRgb(60, 100, 100);
    auto green = hsvRgb(120, 100, 100);
    auto cyan = hsvRgb(180, 100, 100);
    auto blue = hsvRgb(240, 100, 100);
    assert(red.r == 255 && red.g == 0 && red.b == 0);
    assert(yellowEdge.r == 255 && yellowEdge.g >= 250 && yellowEdge.b == 0);
    assert(yellow.r == 255 && yellow.g == 255 && yellow.b == 0);
    assert(green.r == 0 && green.g == 255 && green.b == 0);
    assert(cyan.r == 0 && cyan.g == 255 && cyan.b == 255);
    assert(blue.r == 0 && blue.g == 0 && blue.b == 255);
    auto gray = hsvRgb(311, 0, 50);
    assert(gray.r == gray.g && gray.g == gray.b);
}

static void testSettingsChordNeedsBothButtonsForThreeSeconds() {
    SettingsChord chord;
    assert(!chord.poll(true, false, 0));
    assert(!chord.poll(true, true, 100));
    assert(!chord.poll(true, true, 3099));
    assert(chord.poll(true, true, 3100));
    assert(!chord.poll(true, true, 4000));
    assert(!chord.poll(false, true, 4100));
    assert(!chord.poll(true, true, 5000));
    assert(chord.poll(true, true, 8000));
}

static void testChordConsumptionSuppressesBothReleaseTaps() {
    SettingsChord chord;
    watchinput::ButtonGesture a, b;
    assert(!chord.poll(true, true, 100));
    a.consume(true);
    b.consume(true);
    assert(chord.poll(true, true, 3100));
    a.consume(true);
    b.consume(true);
    assert(a.poll(false, false, true, 3200) == watchinput::Gesture::None);
    assert(b.poll(false, false, true, 3200) == watchinput::Gesture::None);
}

static void testScrollListKeepsSelectionVisibleAndStopsAtEdges() {
    ScrollList list;
    list.configure(7, 4);
    list.select(4);
    assert(list.selected() == 4 && list.first() == 1);
    list.move(2);
    assert(list.selected() == 6 && list.first() == 3);
    list.move(1);
    assert(list.selected() == 6 && list.first() == 3);
    list.select(0);
    list.move(-1);
    assert(list.selected() == 0 && list.first() == 0);
}

static void testMotionFilterDeadzoneHysteresisAndCooldown() {
    MotionFilter filter;
    auto quiet = filter.update(0.05f, -0.08f, 5.0f, 100);
    assert(quiet.x == 0.0f && quiet.y == 0.0f && !quiet.poke);
    for (int i = 0; i < 20; ++i) quiet = filter.update(0.55f, 0.0f, 5.0f, 200 + i * 20);
    assert(quiet.x > 0.35f && quiet.x < 0.60f);
    bool poked = false;
    for (int i = 0; i < 20; ++i) poked |= filter.update(0, 0, 420.0f, 1000 + i * 20).poke;
    assert(poked);
    for (int i = 0; i < 40; ++i) assert(!filter.update(0, 0, 420.0f, 1600 + i * 20).poke);
    for (int i = 0; i < 30; ++i) filter.update(0, 0, 0.0f, 2500 + i * 20);
    for (int i = 0; i < 20; ++i) assert(!filter.update(0, 0, 420.0f, 3500 + i * 20).poke);
    for (int i = 0; i < 30; ++i) filter.update(0, 0, 0.0f, 7800 + i * 20);
    poked = false;
    for (int i = 0; i < 20; ++i) poked |= filter.update(0, 0, 420.0f, 8500 + i * 20).poke;
    assert(poked);
}

static void testAmbientCycleIsSlowAndCanBePostponed() {
    AmbientCycle cycle;
    cycle.begin(1000);
    assert(!cycle.update(28999));
    assert(cycle.update(29000));
    assert(cycle.index() > 0 && cycle.index() < 6);
    uint32_t next = cycle.nextMs();
    assert(next >= 55000 && next <= 77000);
    cycle.postpone(100000);
    assert(!cycle.update(159999));
    assert(cycle.update(160000));
}

int main() {
    testHsvKeyColorsAndSectorBoundary();
    testSettingsChordNeedsBothButtonsForThreeSeconds();
    testChordConsumptionSuppressesBothReleaseTaps();
    testScrollListKeepsSelectionVisibleAndStopsAtEdges();
    testMotionFilterDeadzoneHysteresisAndCooldown();
    testAmbientCycleIsSlowAndCanBePostponed();
    return 0;
}
