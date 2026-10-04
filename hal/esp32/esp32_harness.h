#pragma once

#include "toad/types.h"
#include "toad/engine.h"
#include "toad/display_engine.h"
#include "../common/hal_interface.h"
#include <cstdint>
#include <cstddef>
#include <array>
#include <functional>

namespace toad {

// ============================================================================
// DADAMACHINES TBD-16 / CTAG TBD HARDWARE DEFINITIONS (ESP32-P4/S3)
// ============================================================================
constexpr size_t TBD16_STEP_LEDS   = 16;
constexpr size_t TBD16_ENCODERS    = 4;

struct NeoPixelRGB {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
};

struct Esp32RawHardwareState {
    // 16 Step Buttons
    bool step_buttons[TBD16_STEP_LEDS]{};

    // 4 Rotary Encoders (relative tick delta and push-switch)
    int16_t encoder_delta[TBD16_ENCODERS]{};
    bool encoder_push[TBD16_ENCODERS]{};

    // Tactile Switches
    bool dpad_up{false};
    bool dpad_down{false};
    bool dpad_left{false};
    bool dpad_right{false};
    bool btn_a{false};
    bool btn_b{false};
    bool btn_x{false};
    bool btn_y{false};
    bool screen_left{false};  // Page Prev
    bool screen_right{false}; // Page Next
    bool record_switch{false};
    bool play_switch{false};
};

// ============================================================================
// ESP32 DUAL-CORE HARDWARE ABSTRACTION LAYER (HAL)
// Core 1: I2S Audio DMA Callback
// Core 0: ST7789 SPI LCD DMA, NeoPixel RMT, Encoders, UART MIDI
// ============================================================================
class ESP32HAL : public ITrackerHAL {
public:
    ESP32HAL();
    ~ESP32HAL() override;

    bool initialize(uint32_t sampleRate, size_t bufferSize) override;
    void shutdown() override;
    void pollInput(std::function<void(const InputEvent&)> callback) override;
    void renderFrame(const uint16_t* rgb565, size_t width, size_t height) override;
    IMidiOutputSink* getMidiSink() override;

    // Direct hardware injection (for testing and ESP-IDF peripheral drivers)
    void injectHardwareState(const Esp32RawHardwareState& raw);

    // Core 1 Real-time Audio DMA Callback Simulator / Processor
    void processAudioDmaBlock(float* i2sStereoDmaBuffer, size_t numFrames, Engine& engine);

    // Core 0 NeoPixel Step LED Updater
    void updateStepLeds(uint8_t activeStep, uint8_t playingStep, bool isPlaying);

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    [[nodiscard]] const std::array<NeoPixelRGB, TBD16_STEP_LEDS>& getStepLeds() const noexcept { return stepLeds_; }
    [[nodiscard]] const uint16_t* getSpiDmaBuffer() const noexcept { return spiLcdBuffer_.data(); }
    [[nodiscard]] uint32_t getSampleRate() const noexcept { return sampleRate_; }

private:
    bool initialized_{false};
    uint32_t sampleRate_{44100};
    size_t bufferSize_{128};

    Esp32RawHardwareState lastHwState_{};
    std::array<NeoPixelRGB, TBD16_STEP_LEDS> stepLeds_{};
    std::array<uint16_t, DisplayEngine::SCREEN_WIDTH * DisplayEngine::SCREEN_HEIGHT> spiLcdBuffer_{};

    class UartMidiSink : public IMidiOutputSink {
    public:
        void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t sampleOffset) override {
            bytes_.push_back(static_cast<uint8_t>(0x90 | (channel & 0x0F)));
            bytes_.push_back(note & 0x7F);
            bytes_.push_back(velocity & 0x7F);
            (void)sampleOffset;
        }
        void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t sampleOffset) override {
            bytes_.push_back(static_cast<uint8_t>(0x80 | (channel & 0x0F)));
            bytes_.push_back(note & 0x7F);
            bytes_.push_back(velocity & 0x7F);
            (void)sampleOffset;
        }
        void sendControlChange(uint8_t channel, uint8_t controller, uint8_t value, uint32_t sampleOffset) override {
            bytes_.push_back(static_cast<uint8_t>(0xB0 | (channel & 0x0F)));
            bytes_.push_back(controller & 0x7F);
            bytes_.push_back(value & 0x7F);
            (void)sampleOffset;
        }
        void sendPitchBend(uint8_t channel, uint16_t bendValue, uint32_t sampleOffset) override {
            bytes_.push_back(static_cast<uint8_t>(0xE0 | (channel & 0x0F)));
            bytes_.push_back(static_cast<uint8_t>(bendValue & 0x7F));
            bytes_.push_back(static_cast<uint8_t>((bendValue >> 7) & 0x7F));
            (void)sampleOffset;
        }
        void clear() override { bytes_.clear(); }
        [[nodiscard]] const std::vector<uint8_t>& getBytes() const noexcept { return bytes_; }

    private:
        std::vector<uint8_t> bytes_;
    };

    UartMidiSink uartMidiSink_{};
};

} // namespace toad
