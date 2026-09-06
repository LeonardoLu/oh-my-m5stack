#include "Power.h"
#include <M5Unified.h>

void Power::begin() {
    // Disable only single-click reset; leave double-off and download intact.
    uint8_t cfg=0, verify=0;
    auto& pm=M5.Power.M5pm1;
    pm.readRegister(0x4a,&_bootOffCfg,1);
    bool readKey=pm.readRegister(0x49,&cfg,1); _bootKeyCfg=cfg;
    _homeKeyReady=readKey
        && pm.writeRegister8(0x49,cfg|0x01)
        && pm.readRegister(0x49,&verify,1) && verify==(uint8_t)(cfg|0x01);
    pm.clearButtonIRQStatus();
    setIndicator(false);
    applyLevel(3);
    update();
}

void Power::update() {
    const int mv = M5.Power.getBatteryVoltage();
    if (mv >= 3000 && mv <= 4400) {
        _filteredMv = (_filteredMv == 0)
            ? (uint16_t)mv
            : (uint16_t)((_filteredMv * 7U + (uint16_t)mv) / 8U);
        int pct = ((int)_filteredMv - 3300) * 100 / 900;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        _batteryPct = (uint8_t)pct;
    }

    // M5Unified reads M5PM1 GPIO2 active-low for StopWatch. Gate it with VIN
    // so a floating/status-low line cannot claim charging when unplugged.
    const int vinMv = M5.Power.getVBUSVoltage();
    _charging = vinMv > 4000
        && M5.Power.isCharging() == m5::Power_Class::is_charging;
}

uint8_t Power::levelToValue(uint8_t level) {
    if (level < 1) level = 1;
    if (level > 5) level = 5;
    static const uint8_t kMap[5] = { 32, 72, 120, 180, 245 };
    return kMap[level - 1];
}

void Power::setBrightness(uint8_t v) {
    M5.Display.setBrightness(v);
}

void Power::applyLevel(uint8_t level) {
    setBrightness(levelToValue(level));
}

bool Power::setIndicator(bool enabled) {
    if (_indicatorReady && _indicator==enabled) return true;
    uint8_t cfg=0;
    auto& pm=M5.Power.M5pm1;
    _indicatorReady=pm.setLedEnLevel(enabled) && pm.readRegister(0x06,&cfg,1)
        && ((cfg&0x10)!=0)==enabled;
    if (_indicatorReady) _indicator=enabled;
    return _indicatorReady;
}
