#include "AudioFeedback.h"

#include <M5Unified.h>

void AudioFeedback::_tone(uint16_t frequency, uint16_t durationMs)
{
    if (_enabled) M5.Speaker.tone(static_cast<float>(frequency), durationMs);
}

void AudioFeedback::select()  { _tone(900, 18); }
void AudioFeedback::action()  { _tone(1120, 28); }
void AudioFeedback::confirm() { _tone(1320, 65); }
void AudioFeedback::reject()  { _tone(380, 85); }
void AudioFeedback::error()   { _tone(300, 55); }
