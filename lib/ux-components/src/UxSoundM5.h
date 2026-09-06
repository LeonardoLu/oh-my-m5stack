#pragma once
#include "UxSound.h"
#include <atomic>
#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace ux { namespace sound {
// Keep this output and its speaker/source alive for the lifetime of the device.
// Only the host calls update()/Source methods. Only the sender calls playRaw().
// SDK queue publication can wait indefinitely, so it must never run on the UI.
template<class Speaker> class M5Output {
public:
    M5Output() = default;
    M5Output(const M5Output&) = delete;
    M5Output& operator=(const M5Output&) = delete;
    static constexpr size_t ChunkSamples = 2048; // 128 ms at 16 kHz
    bool begin(Speaker& speaker, Source& source, uint8_t channel = 6) {
        if (_speaker || channel >= 8) return false;
        if (!speaker.begin() || !speaker.isRunning() || !speaker.isEnabled()) return false;
        _speaker = &speaker; _source = &source; _channel = channel;
#if defined(ESP_PLATFORM)
        // ESP-IDF task stack sizes are bytes. This task never synthesizes audio.
        if (xTaskCreate(_taskEntry, "ux-audio-send", 2048, this, 1, &_sender) != pdPASS) {
            _speaker = nullptr; _source = nullptr; return false;
        }
#endif
        return true;
    }
    bool setSource(Source& source) {
        if (!_speaker || busy()) return false;
        _source = &source; return true;
    }
    // Bounded synthesis only: no Speaker call, queue wait, or frame-time heap.
    // Call every host loop, at most 100 ms apart during normal streaming.
    void update() {
        if (!_speaker || !_source) return;
        for (unsigned generated = 0; generated < 2; ++generated) {
            if (_inFlight.load(std::memory_order_acquire) >= 2 || !_source->active()) break;
            Slot& slot = _slots[_produceSequence % 3];
            if (slot.state.load(std::memory_order_acquire) != Free) break;
            slot.length = _source->render(slot.samples, ChunkSamples);
            if (!slot.length) break;
            // The worker can retire old buffers concurrently, but the sole
            // producer is the only task that increases the ahead count.
            _inFlight.fetch_add(1, std::memory_order_release);
            slot.state.store(Ready, std::memory_order_release);
            ++_produceSequence;
#if defined(ESP_PLATFORM)
            xTaskNotifyGive(_sender);
#endif
        }
    }
    bool busy() const { return _source && (_source->active() || _inFlight.load(std::memory_order_acquire)); }
    void cancel() { if (_source) _source->cancel(); }
    uint32_t failures() const { return _failures.load(std::memory_order_relaxed); }
#if !defined(ESP_PLATFORM)
    // Host tests run this on their simulated sender, never on the UI producer.
    void transportStep() { _serviceTransport(); }
#endif
private:
    enum State : uint8_t { Free, Ready, Submitted };
    struct Slot {
        std::atomic<uint8_t> state{Free};
        size_t length = 0;
        int16_t samples[ChunkSamples]{};
    };
    void _retireCompleted() {
        // No other producer may use this channel. The count includes published
        // AND playing slots. A decrease retires successful submissions in FIFO
        // order; an in-progress playRaw is never retired until it returns.
        const size_t occupied = _speaker->isPlaying(_channel);
        // SDK slot retirement is a release store, but isPlaying reads relaxed.
        // Pair those observed retirements before publishing our buffers Free.
        std::atomic_thread_fence(std::memory_order_acquire);
        while (_submittedCount > occupied) {
            const uint8_t index = _submitted[_submittedHead];
            _submittedHead = (_submittedHead + 1) % 2;
            --_submittedCount;
            _slots[index].state.store(Free, std::memory_order_release);
            _inFlight.fetch_sub(1, std::memory_order_release);
        }
    }
    void _serviceTransport() {
        if (!_speaker) return;
        _retireCompleted();
        for (unsigned sent = 0; sent < 2 && _submittedCount < 2; ++sent) {
            const uint8_t index = _sendSequence % 3;
            Slot& slot = _slots[index];
            uint8_t ready = Ready;
            if (!slot.state.compare_exchange_strong(ready, Submitted, std::memory_order_acquire)) break;
            // isPlaying()<2 does NOT prove flip is writable: one published
            // request may still await adoption. Only this worker may wait here.
            if (!_speaker->playRaw(slot.samples, slot.length, SampleRate, false, 1, _channel, false)) {
                _failures.fetch_add(1, std::memory_order_relaxed);
                slot.state.store(Ready, std::memory_order_release);
                break; // retry identical samples; do not advance Source again
            }
            _submitted[(_submittedHead + _submittedCount) % 2] = index;
            ++_submittedCount;
            ++_sendSequence;
            _retireCompleted();
        }
    }
#if defined(ESP_PLATFORM)
    static void _taskEntry(void* context) {
        auto* self = static_cast<M5Output*>(context);
        for (;;) {
            self->_serviceTransport();
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2) ? pdMS_TO_TICKS(2) : 1);
        }
    }
    TaskHandle_t _sender = nullptr;
#endif
    Speaker* _speaker = nullptr;
    Source* _source = nullptr;
    uint8_t _channel = 6;
    uint32_t _produceSequence = 0; // host-owned
    uint32_t _sendSequence = 0;    // sender-owned
    uint8_t _submitted[2]{}, _submittedHead = 0, _submittedCount = 0;
    std::atomic<uint8_t> _inFlight{0};
    std::atomic<uint32_t> _failures{0};
    Slot _slots[3]{};
};
} }
