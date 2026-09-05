#pragma once

#include <stdint.h>

class AudioFeedback {
public:
    void setEnabled(bool enabled) { _enabled = enabled; }
    void select();
    void action();
    void confirm();
    void reject();
    void error();

private:
    void _tone(uint16_t frequency, uint16_t durationMs);
    bool _enabled = true;
};
