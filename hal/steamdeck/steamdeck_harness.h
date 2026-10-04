#pragma once

#include "toad/types.h"
#include "toad/engine.h"
#include "toad/display_engine.h"
#include "toad/ui_state.h"
#include "../common/hal_interface.h"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>

namespace toad {

// ============================================================================
// STEAM DECK CONTROLLER RAW STATE & JOG ACCUMULATOR
// ============================================================================
struct SteamDeckRawInput {
    // Buttons (0 = released, 1 = pressed)
    bool l1{false};
    bool r1{false};
    bool dpad_up{false};
    bool dpad_down{false};
    bool dpad_left{false};
    bool dpad_right{false};
    bool btn_a{false};
    bool btn_b{false};
    bool btn_x{false};
    bool btn_y{false};
    bool start{false};
    bool select{false};
    bool l2_trigger{false};
    bool r2_trigger{false};
    bool l4_grip{false};
    bool l5_grip{false};
    bool r4_grip{false};
    bool r5_grip{false};

    // Trackpads (normalized -1.0 .. 1.0, and touch flag)
    bool right_trackpad_touch{false};
    float right_trackpad_x{0.0f};
    float right_trackpad_y{0.0f};
};

// ============================================================================
// STEAM DECK HARDWARE ABSTRACTION LAYER (HAL)
// Handles 1280x800 display, PipeWire audio, and controller input mapping
// ============================================================================
class SteamDeckHAL : public ITrackerHAL {
public:
    SteamDeckHAL();
    ~SteamDeckHAL() override;

    bool initialize(uint32_t sampleRate, size_t bufferSize) override;
    void shutdown() override;
    void pollInput(std::function<void(const InputEvent&)> callback) override;
    void renderFrame(const uint16_t* rgb565, size_t width, size_t height) override;
    IMidiOutputSink* getMidiSink() override;

    // Direct input injection (for testing and native driver hooks)
    void injectRawInput(const SteamDeckRawInput& raw);
    void processTrackpadJog(float currentAngle, std::function<void(const InputEvent&)>& callback);

    [[nodiscard]] const uint32_t* getCompositedFrame() const noexcept { return frameBuffer1280x800_.data(); }
    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    [[nodiscard]] uint32_t getSampleRate() const noexcept { return sampleRate_; }

private:
    bool initialized_{false};
    uint32_t sampleRate_{44100};
    size_t bufferSize_{128};

    SteamDeckRawInput lastRawState_{};
    float lastTrackpadAngle_{0.0f};
    bool trackpadWasTouched_{false};

    std::vector<uint32_t> frameBuffer1280x800_{};

    class SteamDeckMidiSink : public IMidiOutputSink {
    public:
        void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t sampleOffset) override {
            messages_.push_back({static_cast<uint8_t>(0x90 | (channel & 0x0F)), note, velocity, sampleOffset});
        }
        void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t sampleOffset) override {
            messages_.push_back({static_cast<uint8_t>(0x80 | (channel & 0x0F)), note, velocity, sampleOffset});
        }
        void sendControlChange(uint8_t channel, uint8_t controller, uint8_t value, uint32_t sampleOffset) override {
            messages_.push_back({static_cast<uint8_t>(0xB0 | (channel & 0x0F)), controller, value, sampleOffset});
        }
        void sendPitchBend(uint8_t channel, uint16_t bendValue, uint32_t sampleOffset) override {
            uint8_t lsb = bendValue & 0x7F;
            uint8_t msb = (bendValue >> 7) & 0x7F;
            messages_.push_back({static_cast<uint8_t>(0xE0 | (channel & 0x0F)), lsb, msb, sampleOffset});
        }
        void clear() override {
            messages_.clear();
        }

        [[nodiscard]] const std::vector<MidiMessage>& getMessages() const noexcept { return messages_; }

    private:
        std::vector<MidiMessage> messages_;
    };

    SteamDeckMidiSink midiSink_{};
};

} // namespace toad
