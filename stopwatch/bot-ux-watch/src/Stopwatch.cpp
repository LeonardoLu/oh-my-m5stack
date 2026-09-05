#include "Stopwatch.h"
#include <stdio.h>

void Stopwatch::begin() {
    _state = State::Idle;
    _now = 0;
    _accumMs = 0;
    _startMs = 0;
    _lapCount = 0;
    _totalLaps = 0;
}

void Stopwatch::update(uint32_t nowMs) { _now = nowMs; }

uint32_t Stopwatch::elapsedMs() const {
    uint32_t e = _accumMs;
    if (_state == State::Running) e += (_now - _startMs);
    return e;
}

void Stopwatch::pressPrimary() {
    switch (_state) {
        case State::Idle:      // start
            _startMs = _now;
            _state = State::Running;
            break;
        case State::Running:   // lap (keeps running)
            _recordLap();
            break;
        case State::Stopped:   // reset
            begin();
            break;
    }
}

void Stopwatch::pressSecondary() {
    switch (_state) {
        case State::Running:   // stop
            _accumMs += (_now - _startMs);
            _state = State::Stopped;
            break;
        case State::Stopped:   // resume
            _startMs = _now;
            _state = State::Running;
            break;
        default: break;        // Idle: ignore
    }
}

void Stopwatch::_recordLap() {
    // keep the newest lap at [0]; older laps shift right and drop off
    _laps[2] = _laps[1];
    _laps[1] = _laps[0];
    _laps[0] = elapsedMs();
    _lapNum[2] = _lapNum[1];
    _lapNum[1] = _lapNum[0];
    _lapNum[0] = ++_totalLaps;
    if (_lapCount < 3) _lapCount++;
}

void Stopwatch::draw(M5Canvas* cv, uint16_t accent, uint16_t text) {
    uint32_t e = elapsedMs();
    uint32_t cs = e / 10;              // centiseconds
    uint32_t m = cs / 6000;
    uint32_t s = (cs / 100) % 60;
    uint32_t c = cs % 100;

    char buf[24];
    cv->setTextDatum(middle_center);
    cv->setTextSize(4.0f);
    cv->setTextColor(accent);
    snprintf(buf, sizeof(buf), "%02u:%02u.%02u", (unsigned)m, (unsigned)s, (unsigned)c);
    cv->drawString(buf, cv->width() / 2, 200);

    cv->setTextSize(1.5f);
    cv->setTextColor(text);
    for (uint8_t i = 0; i < _lapCount; i++) {
        uint32_t lcs = _laps[i] / 10;
        snprintf(buf, sizeof(buf), "L%u  %02u:%02u.%02u",
                 (unsigned)_lapNum[i],
                 (unsigned)(lcs / 6000),
                 (unsigned)((lcs / 100) % 60),
                 (unsigned)(lcs % 100));
        cv->drawString(buf, cv->width() / 2, 300 + i * 36);
    }
}
