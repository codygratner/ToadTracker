#pragma once

#include "toad/types.h"
#include "toad/engine.h"
#include "toad/display_engine.h"
#include "../common/hal_interface.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>

namespace toad {

// ============================================================================
// RASPBERRY PI LINUX ARM DISPLAY MODE
// ============================================================================
enum RpiDisplayMode : uint8_t {
    RPI_DISP_SPI_ST7789 = 0, // Direct 240x240 SPI LCD (/dev/spidev0.0)
    RPI_DISP_HDMI_3X,        // HDMI / DRM 720x720 centered on 1080p
    RPI_DISP_HDMI_4X         // HDMI / DRM 960x960 centered on 1080p
};

// ============================================================================
// RASPBERRY PI RAW INPUT (Evdev Gamepad & GPIO Matrix)
// ============================================================================
struct RpiRawInput {
    // Gamepad buttons
    bool btn_l1{false};
    bool btn_r1{false};
    bool dpad_up{false};
    bool dpad_down{false};
    bool dpad_left{false};
    bool dpad_right{false};
    bool btn_a{false};
    bool btn_b{false};
    bool btn_x{false};
    bool btn_y{false};
    bool btn_start{false};
    bool btn_select{false};
    bool trig_l2{false};
    bool trig_r2{false};

    // Direct GPIO switch pins (matching 8-button minimum)
    bool gpio_left{false};
    bool gpio_right{false};
    bool gpio_up{false};
    bool gpio_down{false};
    bool gpio_a{false};
    bool gpio_b{false};
    bool gpio_opt{false};
    bool gpio_edit{false};
};

// ============================================================================
// RASPBERRY PI HARDWARE ABSTRACTION LAYER (HAL)
// Low-latency ALSA audio, SPI/HDMI display, and evdev/GPIO controller
// ============================================================================
class RaspberryPiHAL : public ITrackerHAL {
public:
    explicit RaspberryPiHAL(RpiDisplayMode displayMode = RPI_DISP_SPI_ST7789);
    ~RaspberryPiHAL() override;

    bool initialize(uint32_t sampleRate, size_t bufferSize) override;
    void shutdown() override;
    void pollInput(std::function<void(const InputEvent&)> callback) override;
    void renderFrame(const uint16_t* rgb565, size_t width, size_t height) override;
    IMidiOutputSink* getMidiSink() override;

    // Direct input injection (for unit tests and Linux evdev driver)
    void injectRawInput(const RpiRawInput& raw);
    void setDisplayMode(RpiDisplayMode mode);

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    [[nodiscard]] RpiDisplayMode getDisplayMode() const noexcept { return displayMode_; }
    [[nodiscard]] const std::vector<uint16_t>& getSpiFrameBuffer() const noexcept { return spiBuffer240x240_; }
    [[nodiscard]] const std::vector<uint32_t>& getHdmiFrameBuffer() const noexcept { return hdmiBuffer1080p_; }
    [[nodiscard]] uint32_t getSampleRate() const noexcept { return sampleRate_; }

    // ARM FPU flush-to-zero configuration
    static void configureArmFlushToZero();

private:
    bool initialized_{false};
    uint32_t sampleRate_{44100};
    size_t bufferSize_{128};
    RpiDisplayMode displayMode_{RPI_DISP_SPI_ST7789};

    RpiRawInput lastRawState_{};
    std::vector<uint16_t> spiBuffer240x240_;
    std::vector<uint32_t> hdmiBuffer1080p_;

    class AlsaMidiSink : public IMidiOutputSink {
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
            messages_.push_back({static_cast<uint8_t>(0xE0 | (channel & 0x0F)),
                                 static_cast<uint8_t>(bendValue & 0x7F),
                                 static_cast<uint8_t>((bendValue >> 7) & 0x7F),
                                 sampleOffset});
        }
        void clear() override { messages_.clear(); }
        [[nodiscard]] const std::vector<MidiMessage>& getMessages() const noexcept { return messages_; }

    private:
        std::vector<MidiMessage> messages_;
    };

    AlsaMidiSink alsaMidiSink_{};
};

} // namespace toad
