# UX sound

`UxSound.h` is the hardware-independent synthesis product. It does not call
Arduino `tone`, allocate a task, read a wall clock, or allocate memory. The host
advances sample time by rendering. `UxSoundM5.h` adapts that source to the existing
M5Unified speaker through one isolated sender task. `UxSoundPcm.h` is an optional, separately linked PCM player.
All sound code and the generated sine table are original MIT-licensed component
code; there are no licensed recordings or built-in audio asset bundles.

## Device integration

```cpp
#include <M5Unified.h>
#include <UxSound.h>
#include <UxSoundM5.h>

ux::sound::Synth sounds;
ux::sound::M5Output<m5::Speaker_Class> soundOutput;

// After M5.begin():
// bool ready = soundOutput.begin(M5.Speaker, sounds, 6);
// sounds.setEnabled(settings.sound);
// sounds.setVolume(180); // 0..255, default 180; independent of M5 master volume

// On an accepted action (not both press and release):
// sounds.play(ux::sound::Cue::Confirm);

// Every loop, including while drawing settings:
// soundOutput.update();
// auto failures = soundOutput.failures(); // cumulative telemetry counter
```

Reserve the chosen virtual channel for this output and call it from only one
producer. The adapter never changes the device's speaker pins, master gain,
sample-rate configuration, or any other channel. `begin` verifies the speaker is
running/enabled; it must not be called for every cue. Objects and source storage
must outlive queued playback. Do not copy a live output object.

Call `update()` at least every 100 ms during playback. Three fixed buffers of
2,048 signed 16-bit mono samples consume **12,288 bytes**, plus small bookkeeping.
At 16 kHz, a chunk lasts 128 ms. M5 holds a playing slot plus one queued slot.
Even when the playing slot is almost exhausted, a queued full chunk covers a
100 ms producer pause. Merely using two 64 ms slots would not guarantee a 90 ms
pause at every queue phase. Short cues submit only their actual sample count;
a 28 ms tap is not padded to 128 ms.

`update()` only renders the Source on the host task. It never calls the speaker
SDK. A dedicated sender task (2,048-byte stack, allocated once by `begin`) owns
`playRaw` and completion polling. Buffers use atomic Free → Ready → Submitted →
Free ownership and FIFO submission/retirement. An acquire fence after the SDK
occupancy snapshot pairs its relaxed slot reads with the speaker task's release
retirement, before the UI is allowed to reuse the borrowed samples. At most two Ready/Submitted blocks
exist ahead of playback; the third buffer is only reusable after FIFO completion.
The sender never accesses the Source, so naming, cue replacement and synthesis
remain single-owner host operations. Keep this output alive until device reset;
there is intentionally no unsafe task deletion while an SDK call holds state.

A crucial SDK distinction: `isPlaying(channel)==1` can mean a **published but not
yet adopted** request. In that state the SDK's current `flip` slot is occupied and
`playRaw` may wait even though the occupied count is less than two. The adapter
therefore does not treat that count as a nonblocking-enqueue guarantee. Any SDK
wait is isolated to the sender; the host remains responsive even if the speaker
consumer stalls. This isolation cannot repair a stalled I2S driver or guarantee
audio continues when that driver stops progressing. `failures()` counts returned
submission failures, not an SDK call that has not returned.

The speaker's I2S/DMA task and buffers remain M5Unified's responsibility. Failed
submissions retain the prepared block for retry. No frame-time allocation occurs
in either adapter task. Normal 100 ms producer-pause coverage assumes the sender
and speaker consumer remain scheduled and the driver is functioning.

`cancel()` and `setEnabled(false)` apply an 8 ms release to material not yet
queued. Already queued material drains naturally: **maximum queued cancellation
or preemption application-queue latency is 256 ms plus release**, under normal device operation; the hardware DMA path can add its own latency.
Muting rejects all new cues immediately. This deliberately avoids an abrupt
speaker `stop()` that would cut a waveform and produce a click. Enabling again
does not replay discarded pending cues. `setVolume` changes are smoothed over
roughly 4 ms in newly synthesized samples; queued samples retain their old gain.

## Sound vocabulary and synthesis

| Cue | Duration | Character |
| --- | --- | --- |
| Tap | 28 ms | Short muted pluck |
| Select | 45 ms | Small upward bell |
| Action | 113 ms | Soft pulse then pluck |
| Confirm | 168 ms | Rising bell/chime |
| Back | 70 ms | Descending sine glide |
| Open / Close | 158 / 128 ms | Mirrored two-note gesture |
| Success | 306 ms | Major arpeggio |
| Warning | 255 ms | Two restrained pulses |
| Error / Reject | 268 / 100 ms | Descending low notes / glide |
| Connected / Disconnected | 281 / 213 ms | Rising / falling interval |
| Wake / Sleep | 274 / 476 ms | Brightening / settling phrase |
| Poke | 160 ms | Playful up/down pitch movement |

Six timbres are available: Sine, Bell, SoftSquare, Pluck, Chime and Noise.
Oscillators use an interpolated 256-entry sine table. Bell uses gently detuned
partials; SoftSquare uses bounded odd harmonics, not a discontinuous square wave.
Pluck and chime combine harmonics with a decaying envelope. Noise is low-pass
smoothed. Harmonics approaching Nyquist are omitted. Attack/release are smooth
cubic ramps; note endpoints are zero, phase restarts happen at zero envelope,
and glides maintain phase continuity. Mixing is conservatively limited to
±15,000 before M5's mixer, leaving headroom. Actual acoustic quality and safe
hardware volume still depend on device speaker configuration and gain.

`Note` supports MIDI pitch, duration (10–2,000 ms), inter-note gap (0–1,000 ms),
timbre, velocity, and semitone glide. `play(notes,count,priority)` copies up to
8 notes immediately; no borrowed sequence pointer is retained. `midiFrequency`
uses standard A4=440 tuning; `scaleMidi(root,degree,Scale)` supports Major, Minor
and Pentatonic scales with octave extension.

```cpp
ux::sound::Note phrase[3];
for (unsigned i = 0; i < 3; ++i) {
    phrase[i].midi = ux::sound::scaleMidi(60, i * 2);
    phrase[i].durationMs = 80;
    phrase[i].timbre = ux::sound::Timbre::Pluck;
}
sounds.play(phrase, 3, ux::sound::Priority::Interaction);
```

Priorities are Ambient < Interaction < Notification < Alert. Lower priorities
cannot interrupt active generated audio or an accepted higher-priority pending
request. Equal/higher priority replaces through the short release. The pending
slot stores only the latest accepted request, so fast tapping cannot create an
unbounded backlog. Error/Warning/Reject default to Alert; connectivity and Success
to Notification; Sleep to Ambient; other cues to Interaction. Already generated
M5 queue tails retain their documented latency and are not retroactively removed.

## Optional short PCM/audio

`PcmPlayer` accepts borrowed raw mono signed or unsigned 8-bit samples, not a WAV
container, MP3, AAC, filesystem handle or network URL. It supports 4–48 kHz source
rates, linearly resamples to 16 kHz and applies smooth endpoint/cancel envelopes,
volume smoothing and the same conservative peak limit. For input above 16 kHz,
pre-filter/band-limit offline below 7.2 kHz; linear interpolation alone is not an
anti-alias filter. Prefer 8 or 16 kHz assets for small firmware size. Buffer lifetime
must include any queue tail and pending preemption. No PCM asset is linked unless
the application references it.

```cpp
#include <UxSoundPcm.h>
ux::sound::PcmPlayer pcm;
// Keep the asset and player alive; declare a real asset in a separate object.
// ux::sound::PcmClip clip;
// clip.data = myUnsigned8BitAsset;
// clip.samples = sizeof(myUnsigned8BitAsset);
// clip.sampleRate = 8000;
// if (!soundOutput.busy() && soundOutput.setSource(pcm)) pcm.play(clip);
// After PCM finishes: soundOutput.setSource(sounds);
```

`setSource` succeeds only when the previous source and queue have drained. It
allows synth and PCM to share the same output buffers. A second reserved channel
is possible if an application explicitly needs simultaneous sources; avoid
uncontrolled layering and account for the extra buffers/mixer headroom.

## Pinned API evidence and validation

Core2 declares M5Unified **0.2.21**; StopWatch pins M5Unified commit
`8530f5377d782e4a25a6c482de2e71c3f75ca8eb`. The inspected installed
`M5Unified/src/utility/Speaker_Class.hpp` documents `playRaw(int16_t*, count, rate,
stereo, repeat, channel, stop_current_sound)`, retained runtime buffer pointers,
three-buffer reuse, eight virtual channels, and `isPlaying(channel)` returning
0/1/2 occupied slots. Its installed `Speaker_Class.cpp::_set_next_wav` can wait for
an occupied publication slot even when the occupied count is one; the adapter
isolates that call in its sender task and reserves its channel accordingly.
The package source also declares default speaker output rate 48 kHz and DMA
configuration (256 samples × 8 buffers); the adapter supplies source rate 16 kHz
and leaves the hardware configuration intact.

Run from repository root:

```sh
mkdir -p /tmp/ux-sound
c++ -std=c++11 -O2 -Wall -Wextra -Werror -Ilib/ux-components/src \
  lib/ux-components/test/sound/sound_test.cpp \
  lib/ux-components/src/UxSound.cpp lib/ux-components/src/UxSoundPcm.cpp \
  -o /tmp/ux-sound/test
/tmp/ux-sound/test /tmp/ux-sound
```

The test exports individual cue WAVs, `ui-cues.wav`, and an actual 8-bit PCM
resampling example, `pcm8-demo.wav`. Checks cover exact sample duration, zero
endpoints, waveform independence from render block size, peak bounds, low-note
continuity, preemption, cancellation, mute, invalid input, signed/unsigned PCM
equivalence, buffer ownership, failed-submit retry, and seven queue phases with
90/100 ms producer pauses followed by repeated 90 ms frames. This mock advances a
sample-clock consumer using the actual two-slot borrowed-buffer contract; it is
not a claim of measured physical speaker fidelity or RTOS timing.

Measured default cue peaks: 2,649–6,227 (16-bit full scale 32,767), without hard
clipping. All exported cue endpoints are zero. The largest cue sample-to-sample
step is 3,885, reflecting its high-frequency harmonics, not a boundary jump.
The low-pitch sine continuity check remains below 1,000/sample. Host waveforms do not establish physical speaker fidelity or a listening
measurement. Native long-frame integration validation belongs to the device
host; physical listening has not been measured by these tests.

ESP32 `-Os -fno-exceptions -fno-rtti` object checks measured 3,869 bytes of synth
text and 965 bytes for optional PCM, with no global data/BSS in either object
(excluding consumer adapter buffers, linker/runtime dependencies). A static
archive linking only Synth omits PcmPlayer entirely. AddressSanitizer and
UndefinedBehaviorSanitizer runs pass the same host test suite.

The separate transport test includes the SDK's published/adoption phase and an
actual blocked sender. While it waits with `isPlaying()==1`, 10,000 host updates
return without waiting or touching the SDK. A concurrent producer/sender/consumer
stress test verifies all bytes across 200 chunks, Source thread ownership,
complete FIFO order and borrowed-buffer lifetime:

```sh
c++ -std=c++11 -O2 -Wall -Wextra -Werror -pthread -Ilib/ux-components/src \
  lib/ux-components/test/sound/sender_test.cpp -o /tmp/ux-sound/sender-test
/tmp/ux-sound/sender-test
```

Host transports use explicit `transportStep()` scheduling; on ESP targets that
method is not exposed and the sender task runs automatically. Existing device APIs
remain `begin/update/busy/cancel/failures/setSource`.
