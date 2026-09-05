#include "Power.h"
#include <M5Unified.h>

void Power::begin() {
    applyLevel(3);
}

uint8_t Power::batteryPct() {
    int v = (int)M5.Power.getBatteryLevel();
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    return (uint8_t)v;
}

bool Power::charging() {
    // No PMIC on the Plus2: isCharging() is an enum, not a bool, and reports
    // charge_unknown unless actually charging. Only the true charge state counts.
    return M5.Power.isCharging() == m5::Power_Class::is_charging;
}

uint8_t Power::levelToValue(uint8_t level) {
    if (level < 1) level = 1;
    if (level > 5) level = 5;
    static const uint8_t kMap[5] = { 51, 102, 153, 204, 255 };
    return kMap[level - 1];
}

void Power::setBrightness(uint8_t v) {
    M5.Display.setBrightness(v);
}

void Power::applyLevel(uint8_t level) {
    setBrightness(levelToValue(level));
}
