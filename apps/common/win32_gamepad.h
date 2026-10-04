#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <xinput.h>
#pragma comment(lib, "xinput.lib")

#include <toad/input.h>
#include <cmath>
#include <functional>

namespace toad {

class Win32Gamepad {
public:
    Win32Gamepad() = default;

    void poll(std::function<void(const InputEvent&)> callback) {
        if (!callback) return;

        XINPUT_STATE state;
        ZeroMemory(&state, sizeof(XINPUT_STATE));

        DWORD result = XInputGetState(0, &state);
        connected_ = (result == ERROR_SUCCESS);
        if (!connected_) {
            lastButtons_ = 0;
            return;
        }

        WORD buttons = state.Gamepad.wButtons;
        BYTE leftTrigger = state.Gamepad.bLeftTrigger;
        BYTE rightTrigger = state.Gamepad.bRightTrigger;
        int16_t thumbLX = state.Gamepad.sThumbLX;
        int16_t thumbLY = state.Gamepad.sThumbLY;
        int16_t thumbRX = state.Gamepad.sThumbRX;
        int16_t thumbRY = state.Gamepad.sThumbRY;

        bool modView = (leftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
        bool modSub  = (rightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);

        auto checkButton = [&](WORD mask, LogicalInput input) {
            bool isDown = (buttons & mask) != 0;
            bool wasDown = (lastButtons_ & mask) != 0;
            if (isDown && !wasDown) {
                InputEvent ev{};
                ev.input = input;
                ev.pressed = true;
                ev.modifier_view = modView;
                ev.modifier_sub = modSub;
                callback(ev);
            }
        };

        // Standard Buttons (edge-triggered)
        checkButton(XINPUT_GAMEPAD_DPAD_UP, INPUT_UP);
        checkButton(XINPUT_GAMEPAD_DPAD_DOWN, INPUT_DOWN);
        checkButton(XINPUT_GAMEPAD_DPAD_LEFT, INPUT_LEFT);
        checkButton(XINPUT_GAMEPAD_DPAD_RIGHT, INPUT_RIGHT);

        checkButton(XINPUT_GAMEPAD_A, INPUT_BTN_A);
        checkButton(XINPUT_GAMEPAD_B, INPUT_BTN_B);
        checkButton(XINPUT_GAMEPAD_X, INPUT_BTN_OPT);
        checkButton(XINPUT_GAMEPAD_Y, INPUT_BTN_EDIT);

        checkButton(XINPUT_GAMEPAD_LEFT_SHOULDER, INPUT_PAGE_PREV);
        checkButton(XINPUT_GAMEPAD_RIGHT_SHOULDER, INPUT_PAGE_NEXT);

        checkButton(XINPUT_GAMEPAD_START, INPUT_TRANSPORT);
        checkButton(XINPUT_GAMEPAD_BACK, INPUT_RECORD);

        // Thumbstick Clicks (Steam Deck Grip Button Equivalents)
        checkButton(XINPUT_GAMEPAD_LEFT_THUMB, INPUT_OCTAVE_DOWN);
        checkButton(XINPUT_GAMEPAD_RIGHT_THUMB, INPUT_OCTAVE_UP);

        // Analog Stick Navigation (with deadzone & repeat limiter)
        constexpr int16_t DEADZONE = 14000;
        bool stickUp = (thumbLY > DEADZONE);
        bool stickDown = (thumbLY < -DEADZONE);
        bool stickLeft = (thumbLX < -DEADZONE);
        bool stickRight = (thumbLX > DEADZONE);

        if (stickUp && !lastStickUp_) {
            InputEvent ev{}; ev.input = INPUT_UP; ev.pressed = true; ev.modifier_view = modView; ev.modifier_sub = modSub; callback(ev);
        }
        if (stickDown && !lastStickDown_) {
            InputEvent ev{}; ev.input = INPUT_DOWN; ev.pressed = true; ev.modifier_view = modView; ev.modifier_sub = modSub; callback(ev);
        }
        if (stickLeft && !lastStickLeft_) {
            InputEvent ev{}; ev.input = INPUT_LEFT; ev.pressed = true; ev.modifier_view = modView; ev.modifier_sub = modSub; callback(ev);
        }
        if (stickRight && !lastStickRight_) {
            InputEvent ev{}; ev.input = INPUT_RIGHT; ev.pressed = true; ev.modifier_view = modView; ev.modifier_sub = modSub; callback(ev);
        }

        lastStickUp_ = stickUp;
        lastStickDown_ = stickDown;
        lastStickLeft_ = stickLeft;
        lastStickRight_ = stickRight;

        // Right Stick Rotary Jog Wheel Simulation
        float rx = static_cast<float>(thumbRX) / 32767.0f;
        float ry = static_cast<float>(thumbRY) / 32767.0f;
        float distSq = rx * rx + ry * ry;

        if (distSq > 0.25f) { // ~50% deflection
            float angle = std::atan2(ry, rx);
            if (rightStickActive_) {
                float delta = angle - lastRightStickAngle_;
                constexpr float PI = 3.14159265358979323846f;
                if (delta > PI) delta -= 2.0f * PI;
                if (delta < -PI) delta += 2.0f * PI;

                constexpr float JOG_THRESHOLD = 0.35f; // ~20 degrees
                if (std::abs(delta) >= JOG_THRESHOLD) {
                    InputEvent jogEv{};
                    jogEv.pressed = true;
                    jogEv.modifier_view = modView;
                    jogEv.modifier_sub = modSub;
                    jogEv.input = (delta > 0.0f) ? INPUT_JOG_CW : INPUT_JOG_CCW;
                    jogEv.jog_delta = (delta > 0.0f) ? 1 : -1;
                    callback(jogEv);
                    lastRightStickAngle_ = angle;
                }
            } else {
                lastRightStickAngle_ = angle;
                rightStickActive_ = true;
            }
        } else {
            rightStickActive_ = false;
        }

        lastButtons_ = buttons;
    }

    [[nodiscard]] bool isConnected() const noexcept { return connected_; }

private:
    bool connected_{false};
    WORD lastButtons_{0};
    bool lastStickUp_{false};
    bool lastStickDown_{false};
    bool lastStickLeft_{false};
    bool lastStickRight_{false};

    bool rightStickActive_{false};
    float lastRightStickAngle_{0.0f};
};

} // namespace toad
