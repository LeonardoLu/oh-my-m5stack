// Stopwatch — elapsed-time state machine (Idle/Running/Stopped) with a 3-lap
// ring buffer. Button semantics per tmp/ux-design.md §B.3.
#pragma once

#include <M5Unified.h>
#include <stdint.h>

class Stopwatch {
public:
    enum class State : uint8_t { Idle, Running, Stopped };

    void begin();
    void update(uint32_t nowMs);

    void pressPrimary();     // A: Idle->start, Running->lap, Stopped->reset
    void pressSecondary();   // B: Running->stop, Stopped->resume

    bool     running() const { return _state == State::Running; }
    State    state() const { return _state; }
    uint32_t elapsedMs() const;
    uint8_t  lapCount() const { return _lapCount; }
    uint32_t lapMs(uint8_t i) const { return _laps[i]; }
    uint16_t lapNumber(uint8_t i) const { return _lapNum[i]; }

    void draw(M5Canvas* cv, uint16_t accent, uint16_t text);

private:
    void _recordLap();

    State    _state = State::Idle;
    uint32_t _now = 0;
    uint32_t _accumMs = 0;   // elapsed while not running
    uint32_t _startMs = 0;   // start of the current run
    uint32_t _laps[3] = {0, 0, 0};   // newest first
    uint16_t _lapNum[3] = {0, 0, 0};
    uint8_t  _lapCount = 0;
    uint16_t _totalLaps = 0;
};
