#include <catch2/catch_test_macros.hpp>
#include "hal/rpi/rpi_harness.h"
#include "hal/esp32/esp32_harness.h"
#include "toad/engine.h"
#include <vector>
#include <algorithm>

TEST_CASE("HAL: Raspberry Pi Linux ARM Initialization and Display Modes", "[hardware_hal]") {
    toad::RaspberryPiHAL rpi(toad::RPI_DISP_SPI_ST7789);

    REQUIRE(rpi.initialize(44100, 128));
    REQUIRE(rpi.isInitialized());
    REQUIRE(rpi.getDisplayMode() == toad::RPI_DISP_SPI_ST7789);

    // Test SPI ST7789 240x240 frame copy
    std::vector<uint16_t> frame240(toad::DisplayEngine::SCREEN_WIDTH * toad::DisplayEngine::SCREEN_HEIGHT, 0x1234);
    rpi.renderFrame(frame240.data(), toad::DisplayEngine::SCREEN_WIDTH, toad::DisplayEngine::SCREEN_HEIGHT);
    REQUIRE(rpi.getSpiFrameBuffer()[0] == 0x1234);

    // Test HDMI 3x and 4x modes
    rpi.setDisplayMode(toad::RPI_DISP_HDMI_3X);
    REQUIRE(rpi.getDisplayMode() == toad::RPI_DISP_HDMI_3X);
    rpi.renderFrame(frame240.data(), toad::DisplayEngine::SCREEN_WIDTH, toad::DisplayEngine::SCREEN_HEIGHT);
    REQUIRE(rpi.getHdmiFrameBuffer().size() == 1920 * 1080);

    rpi.setDisplayMode(toad::RPI_DISP_HDMI_4X);
    REQUIRE(rpi.getDisplayMode() == toad::RPI_DISP_HDMI_4X);
    rpi.renderFrame(frame240.data(), toad::DisplayEngine::SCREEN_WIDTH, toad::DisplayEngine::SCREEN_HEIGHT);
    REQUIRE(rpi.getHdmiFrameBuffer().size() == 1920 * 1080);

    // Test ALSA MIDI Sink
    toad::IMidiOutputSink* midi = rpi.getMidiSink();
    REQUIRE(midi != nullptr);
    midi->sendNoteOn(0, 60, 100, 0);
    midi->sendControlChange(0, 10, 64, 16);
    midi->sendPitchBend(0, 8192, 32);
    midi->sendNoteOff(0, 60, 0, 48);

    rpi.shutdown();
    REQUIRE_FALSE(rpi.isInitialized());
}

TEST_CASE("HAL: Raspberry Pi Evdev and GPIO Input Mapping", "[hardware_hal]") {
    toad::RaspberryPiHAL rpi;
    rpi.initialize(44100, 128);

    // Inject mixed Gamepad & GPIO inputs
    toad::RpiRawInput raw{};
    raw.btn_l1 = true;
    raw.gpio_down = true;
    raw.gpio_a = true;
    raw.btn_start = true;
    rpi.injectRawInput(raw);

    std::vector<toad::LogicalInput> captured;
    rpi.pollInput([&](const toad::InputEvent& ev) {
        captured.push_back(ev.input);
    });

    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_PAGE_PREV) != captured.end());
    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_DOWN) != captured.end());
    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_BTN_A) != captured.end());
    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_TRANSPORT) != captured.end());
}

TEST_CASE("HAL: ESP32 dadamachines TBD-16 Dual-Core Architecture", "[hardware_hal]") {
    toad::ESP32HAL esp;
    toad::Engine engine;
    toad::Song song;
    engine.loadSong(song);

    REQUIRE(esp.initialize(44100, 128));
    REQUIRE(esp.isInitialized());

    // --- Core 1 Audio DMA Processing ---
    std::vector<float> i2sDmaBuffer(128 * 2, 0.0f); // 128 stereo frames
    esp.processAudioDmaBlock(i2sDmaBuffer.data(), 128, engine);

    // --- Core 0 SPI LCD DMA Transfer ---
    std::vector<uint16_t> rgbFrame(toad::DisplayEngine::SCREEN_WIDTH * toad::DisplayEngine::SCREEN_HEIGHT, 0x07E0);
    esp.renderFrame(rgbFrame.data(), toad::DisplayEngine::SCREEN_WIDTH, toad::DisplayEngine::SCREEN_HEIGHT);
    REQUIRE(esp.getSpiDmaBuffer()[0] == 0x07E0);

    // --- Core 0 NeoPixel Step LED Updater ---
    // Test stopped state with active cursor step 2
    esp.updateStepLeds(2, 0, false);
    const auto& leds = esp.getStepLeds();
    REQUIRE(leds.size() == 16);
    // Downbeat 0 has amber marker
    REQUIRE(leds[0].r == 40);
    // Active step 2 has cyber cyan highlight
    REQUIRE(leds[2].b == 255);

    // Test playing state on step 5
    esp.updateStepLeds(2, 5, true);
    // Playing step 5 has bright phosphor/white illumination
    REQUIRE(leds[5].g == 255);

    // --- Core 0 Hardware Input & Encoders ---
    toad::Esp32RawHardwareState hw{};
    hw.screen_left = true;
    hw.dpad_up = true;
    hw.encoder_delta[0] = 1;  // Encoder 1 -> Down
    hw.encoder_delta[2] = 2;  // Encoder 3 -> Scrub CW (+2)
    hw.step_buttons[4] = true;// Step pad tap -> Audition
    esp.injectHardwareState(hw);

    std::vector<toad::LogicalInput> captured;
    int16_t totalJogDelta = 0;
    esp.pollInput([&](const toad::InputEvent& ev) {
        captured.push_back(ev.input);
        if (ev.input == toad::INPUT_JOG_CW) totalJogDelta += ev.jog_delta;
    });

    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_PAGE_PREV) != captured.end());
    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_UP) != captured.end());
    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_DOWN) != captured.end());
    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_JOG_CW) != captured.end());
    REQUIRE(totalJogDelta == 2);
    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_BTN_EDIT) != captured.end());

    // --- Hardware UART MIDI Sink ---
    toad::IMidiOutputSink* uartMidi = esp.getMidiSink();
    REQUIRE(uartMidi != nullptr);
    uartMidi->sendNoteOn(0, 60, 127, 0);
    uartMidi->sendControlChange(0, 74, 90, 0);
    uartMidi->sendPitchBend(0, 8192, 0);
    uartMidi->sendNoteOff(0, 60, 0, 0);

    esp.shutdown();
    REQUIRE_FALSE(esp.isInitialized());
}
