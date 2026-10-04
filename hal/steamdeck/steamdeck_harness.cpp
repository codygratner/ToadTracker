#include "steamdeck_harness.h"
#include <cmath>
#include <algorithm>

namespace toad {

SteamDeckHAL::SteamDeckHAL() {
    frameBuffer1280x800_.assign(DisplayEngine::STEAM_DECK_WIDTH * DisplayEngine::STEAM_DECK_HEIGHT, 0xFF050808);
}

SteamDeckHAL::~SteamDeckHAL() {
    shutdown();
}

bool SteamDeckHAL::initialize(uint32_t sampleRate, size_t bufferSize) {
    sampleRate_ = sampleRate;
    bufferSize_ = bufferSize;
    initialized_ = true;
    return true;
}

void SteamDeckHAL::shutdown() {
    initialized_ = false;
    midiSink_.clear();
}

void SteamDeckHAL::injectRawInput(const SteamDeckRawInput& raw) {
    lastRawState_ = raw;
}

void SteamDeckHAL::pollInput(std::function<void(const InputEvent&)> callback) {
    if (!callback) return;

    // Helper macro/lambda for button events
    auto emit = [&](LogicalInput input, bool pressed) {
        InputEvent ev;
        ev.input = input;
        ev.pressed = pressed;
        ev.modifier_view = lastRawState_.l2_trigger;
        ev.modifier_sub = lastRawState_.r2_trigger;
        callback(ev);
    };

    if (lastRawState_.l1) emit(INPUT_PAGE_PREV, true);
    if (lastRawState_.r1) emit(INPUT_PAGE_NEXT, true);

    if (lastRawState_.dpad_up) emit(INPUT_UP, true);
    if (lastRawState_.dpad_down) emit(INPUT_DOWN, true);
    if (lastRawState_.dpad_left) emit(INPUT_LEFT, true);
    if (lastRawState_.dpad_right) emit(INPUT_RIGHT, true);

    if (lastRawState_.btn_a) emit(INPUT_BTN_A, true);
    if (lastRawState_.btn_b) emit(INPUT_BTN_B, true);
    if (lastRawState_.btn_x) emit(INPUT_BTN_OPT, true);
    if (lastRawState_.btn_y) emit(INPUT_BTN_EDIT, true);

    if (lastRawState_.start) emit(INPUT_TRANSPORT, true);
    if (lastRawState_.select) emit(INPUT_RECORD, true);

    // Rear Grip Buttons (Steam Deck Specific)
    if (lastRawState_.l4_grip) emit(INPUT_OCTAVE_DOWN, true);
    if (lastRawState_.l5_grip) emit(INPUT_OCTAVE_UP, true);
    if (lastRawState_.r4_grip) emit(INPUT_TRACK_MUTE, true);
    if (lastRawState_.r5_grip) emit(INPUT_TRACK_SOLO, true);

    // Trackpad Rotary Jog Wheel
    if (lastRawState_.right_trackpad_touch) {
        float x = lastRawState_.right_trackpad_x;
        float y = lastRawState_.right_trackpad_y;
        float distSq = x * x + y * y;

        // Deadzone threshold (avoid center noise)
        if (distSq > 0.05f) {
            float angle = std::atan2(y, x);
            if (trackpadWasTouched_) {
                float deltaAngle = angle - lastTrackpadAngle_;
                // Handle angle wrap-around
                constexpr float PI = 3.14159265358979323846f;
                if (deltaAngle > PI) deltaAngle -= 2.0f * PI;
                if (deltaAngle < -PI) deltaAngle += 2.0f * PI;

                // Threshold per tick (~15 degrees = 0.26 radians)
                constexpr float STEP_THRESHOLD = 0.2618f;
                if (std::abs(deltaAngle) >= STEP_THRESHOLD) {
                    InputEvent jogEv;
                    jogEv.pressed = true;
                    jogEv.modifier_view = lastRawState_.l2_trigger;
                    jogEv.modifier_sub = lastRawState_.r2_trigger;

                    if (deltaAngle > 0.0f) {
                        jogEv.input = INPUT_JOG_CW;
                        jogEv.jog_delta = 1;
                    } else {
                        jogEv.input = INPUT_JOG_CCW;
                        jogEv.jog_delta = -1;
                    }
                    callback(jogEv);
                    lastTrackpadAngle_ = angle;
                }
            } else {
                lastTrackpadAngle_ = angle;
                trackpadWasTouched_ = true;
            }
        }
    } else {
        trackpadWasTouched_ = false;
    }
}

void SteamDeckHAL::renderFrame(const uint16_t* rgb565, size_t width, size_t height) {
    if (!rgb565 || width != DisplayEngine::SCREEN_WIDTH || height != DisplayEngine::SCREEN_HEIGHT) return;

    // Convert RGB565 to 32-bit ARGB and 3x scale into 1280x800 framebuffer
    for (size_t y = 0; y < height; ++y) {
        size_t srcRow = y * width;
        size_t dstYBase = DisplayEngine::STEAM_DECK_OFFSET_Y + y * DisplayEngine::STEAM_DECK_SCALE;

        for (size_t scaleY = 0; scaleY < DisplayEngine::STEAM_DECK_SCALE; ++scaleY) {
            size_t dstRow = (dstYBase + scaleY) * DisplayEngine::STEAM_DECK_WIDTH + DisplayEngine::STEAM_DECK_OFFSET_X;
            for (size_t x = 0; x < width; ++x) {
                uint16_t c565 = rgb565[srcRow + x];
                uint8_t r = static_cast<uint8_t>(((c565 >> 11) & 0x1F) * 255 / 31);
                uint8_t g = static_cast<uint8_t>(((c565 >> 5) & 0x3F) * 255 / 63);
                uint8_t b = static_cast<uint8_t>((c565 & 0x1F) * 255 / 31);
                uint32_t argb = 0xFF000000 | (r << 16) | (g << 8) | b;

                for (size_t scaleX = 0; scaleX < DisplayEngine::STEAM_DECK_SCALE; ++scaleX) {
                    frameBuffer1280x800_[dstRow + x * DisplayEngine::STEAM_DECK_SCALE + scaleX] = argb;
                }
            }
        }
    }
}

IMidiOutputSink* SteamDeckHAL::getMidiSink() {
    return &midiSink_;
}

} // namespace toad
