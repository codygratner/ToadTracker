#pragma once

#include "toad/types.h"
#include "toad/input.h"
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <array>
#include <functional>

namespace toad {

// ============================================================================
// SAMPLE-ACCURATE MIDI OUTPUT SINK INTERFACE
// ============================================================================
struct MidiMessage {
    uint8_t status{0};
    uint8_t data1{0};
    uint8_t data2{0};
    uint32_t sample_offset{0};
};

class IMidiOutputSink {
public:
    virtual ~IMidiOutputSink() = default;

    virtual void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t sampleOffset) = 0;
    virtual void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t sampleOffset) = 0;
    virtual void sendControlChange(uint8_t channel, uint8_t controller, uint8_t value, uint32_t sampleOffset) = 0;
    virtual void sendPitchBend(uint8_t channel, uint16_t bendValue, uint32_t sampleOffset) = 0;
    virtual void clear() = 0;
};

// ============================================================================
// SINGLE-PRODUCER SINGLE-CONSUMER (SPSC) LOCK-FREE RING BUFFER
// Strict zero-allocation inter-thread communication (Audio Thread -> UI Thread)
// ============================================================================
template <typename T, size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    SpscRingBuffer() : head_(0), tail_(0) {}

    // Producer method (Audio callback / Hot path)
    bool push(const T& item) noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        if ((current_head - current_tail) >= Capacity) {
            return false; // Buffer full
        }

        buffer_[current_head & Mask] = item;
        head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

    // Consumer method (UI / Display task)
    bool pop(T& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire);

        if (current_tail == current_head) {
            return false; // Buffer empty
        }

        item = buffer_[current_tail & Mask];
        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool isEmpty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool isFull() const noexcept {
        return (head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_acquire)) >= Capacity;
    }

    [[nodiscard]] size_t size() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return (head >= tail) ? (head - tail) : (Capacity - (tail - head));
    }

    void reset() noexcept {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr size_t Mask = Capacity - 1;
    alignas(64) std::array<T, Capacity> buffer_{};
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};

// ============================================================================
// HARDWARE ABSTRACTION LAYER (HAL) INTERFACE
// ============================================================================
class ITrackerHAL {
public:
    virtual ~ITrackerHAL() = default;

    virtual bool initialize(uint32_t sampleRate, size_t bufferSize) = 0;
    virtual void shutdown() = 0;
    virtual void pollInput(std::function<void(const InputEvent&)> callback) = 0;
    virtual void renderFrame(const uint16_t* rgb565, size_t width, size_t height) = 0;
    virtual IMidiOutputSink* getMidiSink() = 0;
};

} // namespace toad
