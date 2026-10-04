#include "esp32_harness.h"
#include <algorithm>

namespace toad {

ESP32HAL::ESP32HAL() {
    spiLcdBuffer_.fill(0x0821);
    stepLeds_.fill({0, 0, 0});
}

ESP32HAL::~ESP32HAL() {
    shutdown();
}

bool ESP32HAL::initialize(uint32_t sampleRate, size_t bufferSize) {
    sampleRate_ = sampleRate;
    bufferSize_ = bufferSize;
    initialized_ = true;
    return true;
}

void ESP32HAL::shutdown() {
    initialized_ = false;
    uartMidiSink_.clear();
    stepLeds_.fill({0, 0, 0});
}

void ESP32HAL::injectHardwareState(const Esp32RawHardwareState& raw) {
    lastHwState_ = raw;
}

void ESP32HAL::processAudioDmaBlock(float* i2sStereoDmaBuffer, size_t numFrames, Engine& engine) {
    if (!i2sStereoDmaBuffer || numFrames == 0) return;
    engine.renderBlockDeterministic(i2sStereoDmaBuffer, numFrames);
}

void ESP32HAL::updateStepLeds(uint8_t activeStep, uint8_t playingStep, bool isPlaying) {
    for (size_t s = 0; s < TBD16_STEP_LEDS; ++s) {
        if (isPlaying && s == playingStep) {
            // Bright white / phosphor green for currently playing step
            stepLeds_[s] = {200, 255, 200};
        } else if (s == activeStep) {
            // Cyber cyan for cursor-selected edit step
            stepLeds_[s] = {0, 180, 255};
        } else if ((s % 4) == 0) {
            // Dim amber marker on downbeats (0, 4, 8, 12)
            stepLeds_[s] = {40, 25, 0};
        } else {
            // Off / dim moss green background
            stepLeds_[s] = {0, 15, 5};
        }
    }
}

void ESP32HAL::pollInput(std::function<void(const InputEvent&)> callback) {
    if (!callback) return;

    auto emit = [&](LogicalInput input, bool pressed) {
        InputEvent ev;
        ev.input = input;
        ev.pressed = pressed;
        callback(ev);
    };

    // Page switches
    if (lastHwState_.screen_left) emit(INPUT_PAGE_PREV, true);
    if (lastHwState_.screen_right) emit(INPUT_PAGE_NEXT, true);

    // Tactile D-pad
    if (lastHwState_.dpad_up) emit(INPUT_UP, true);
    if (lastHwState_.dpad_down) emit(INPUT_DOWN, true);
    if (lastHwState_.dpad_left) emit(INPUT_LEFT, true);
    if (lastHwState_.dpad_right) emit(INPUT_RIGHT, true);

    // A/B/X/Y Tactical switches
    if (lastHwState_.btn_a) emit(INPUT_BTN_A, true);
    if (lastHwState_.btn_b) emit(INPUT_BTN_B, true);
    if (lastHwState_.btn_x) emit(INPUT_BTN_OPT, true);
    if (lastHwState_.btn_y) emit(INPUT_BTN_EDIT, true);

    // Transport Switches
    if (lastHwState_.play_switch) emit(INPUT_TRANSPORT, true);
    if (lastHwState_.record_switch) emit(INPUT_RECORD, true);

    // Encoder 1 & 2 -> Cursor navigation
    if (lastHwState_.encoder_delta[0] > 0) emit(INPUT_DOWN, true);
    else if (lastHwState_.encoder_delta[0] < 0) emit(INPUT_UP, true);

    if (lastHwState_.encoder_delta[1] > 0) emit(INPUT_RIGHT, true);
    else if (lastHwState_.encoder_delta[1] < 0) emit(INPUT_LEFT, true);

    // Encoder 3 & 4 -> Value editing / Jog wheel
    int16_t scrubDelta = lastHwState_.encoder_delta[2] + lastHwState_.encoder_delta[3];
    if (scrubDelta > 0) {
        InputEvent ev;
        ev.input = INPUT_JOG_CW;
        ev.pressed = true;
        ev.jog_delta = scrubDelta;
        callback(ev);
    } else if (scrubDelta < 0) {
        InputEvent ev;
        ev.input = INPUT_JOG_CCW;
        ev.pressed = true;
        ev.jog_delta = scrubDelta;
        callback(ev);
    }

    // Step Pad Taps -> Audition Note
    for (size_t i = 0; i < TBD16_STEP_LEDS; ++i) {
        if (lastHwState_.step_buttons[i]) {
            emit(INPUT_BTN_EDIT, true);
            break;
        }
    }
}

void ESP32HAL::renderFrame(const uint16_t* rgb565, size_t width, size_t height) {
    if (!rgb565 || width != DisplayEngine::SCREEN_WIDTH || height != DisplayEngine::SCREEN_HEIGHT) return;
    // Copy directly into SPI LCD DMA transfer buffer
    std::copy(rgb565, rgb565 + (width * height), spiLcdBuffer_.begin());
}

IMidiOutputSink* ESP32HAL::getMidiSink() {
    return &uartMidiSink_;
}

} // namespace toad
