#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>

#include <toad/version.h>
#include <toad/types.h>
#include <toad/fast_math.h>
#include <toad/engine.h>
#include <toad/ui_state.h>
#include <toad/display_engine.h>
#include <hal/esp32/esp32_harness.h>
#include <hal/common/hal_interface.h>

#include "../common/demo_song.h"
#include "../common/win32_audio.h"
#include "../common/win32_gamepad.h"

#include <memory>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>

namespace toad {

// ============================================================================
// TBD-16 HARDWARE EMULATOR WINDOW DIMENSIONS
// ============================================================================
constexpr int WIN_WIDTH  = 520;
constexpr int WIN_HEIGHT = 770;

// LCD Dimensions (240x240 scaled 2x to 480x480)
constexpr int LCD_SCALE   = 2;
constexpr int LCD_W       = DisplayEngine::SCREEN_WIDTH * LCD_SCALE;  // 480
constexpr int LCD_H       = DisplayEngine::SCREEN_HEIGHT * LCD_SCALE; // 480
constexpr int LCD_OFF_X   = 20;
constexpr int LCD_OFF_Y   = 40;

// Color Constants for Hardware Chassis
constexpr uint32_t COL_CHASSIS_BG   = 0xFF141618;
constexpr uint32_t COL_CHASSIS_RIM  = 0xFF2A2E32;
constexpr uint32_t COL_BEZEL        = 0xFF080A0C;
constexpr uint32_t COL_BTN_FACE     = 0xFF22262B;
constexpr uint32_t COL_BTN_BORDER   = 0xFF3D444D;
constexpr uint32_t COL_BTN_TEXT     = 0xFFCCD6E0;
constexpr uint32_t COL_BTN_ACTIVE   = 0xFF00FF88;
constexpr uint32_t COL_KNOB_BODY    = 0xFF282C32;
constexpr uint32_t COL_KNOB_RIM     = 0xFF444C56;
constexpr uint32_t COL_KNOB_LINE    = 0xFF55FFFF;

// Structure for Clickable Surface Controls
struct RectButton {
    int x, y, w, h;
    const char* label;
    LogicalInput input;
    bool isPressed{false};
    uint32_t accentCol{COL_BTN_BORDER};
};

struct KnobControl {
    int cx, cy, radius;
    const char* label;
    float angle{0.0f};
    int encoderIndex{0};
};

class TBD16EmulatorApp {
public:
    TBD16EmulatorApp() {
        framebuffer_.assign(WIN_WIDTH * WIN_HEIGHT, COL_CHASSIS_BG);
        setupControls();
    }

    bool initialize(HINSTANCE hInstance, int nCmdShow) {
        // Initialize Core Engine & HAL
        engine_.setSampleRate(44100.0f);
        engine_.setBpm(128.0f);
        song_ = createDemoSong();
        engine_.loadSong(song_);

        esp32HAL_.initialize(44100, 512);

        // Register Window Class
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = wndProcStatic;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"ToadTrackerTBD16EmulatorClass";

        if (!RegisterClassExW(&wc)) return false;

        // Calculate Window Rectangle for Client Area
        RECT rc = { 0, 0, WIN_WIDTH, WIN_HEIGHT };
        AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

        hWnd_ = CreateWindowExW(
            0,
            L"ToadTrackerTBD16EmulatorClass",
            L"ToadTracker - dadamachines TBD-16 Hardware Emulator (ESP32-P4)",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, hInstance, this
        );

        if (!hWnd_) return false;

        // Initialize Native Win32 Low-Latency Audio Device
        audioDevice_.start(44100, [this](float* stereoBuffer, size_t frames) {
            esp32HAL_.processAudioDmaBlock(stereoBuffer, frames, engine_);

            // Extract VU levels for 8 tracks
            for (size_t t = 0; t < MAX_TRACKS; ++t) {
                const auto& trk = engine_.getTrackState(t);
                float lvl = trk.voice_active ? (static_cast<float>(trk.effective_volume) / 255.0f) : 0.0f;
                // Smooth decay
                uiState_.vu_levels[t] = std::max(lvl, uiState_.vu_levels[t] * 0.92f);
            }
        });

        ShowWindow(hWnd_, nCmdShow);
        UpdateWindow(hWnd_);

        return true;
    }

    int run() {
        MSG msg{};
        uint32_t lastTick = GetTickCount();

        while (msg.message != WM_QUIT) {
            if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            } else {
                uint32_t now = GetTickCount();
                if (now - lastTick >= 16) { // ~60 Hz Refresh
                    lastTick = now;
                    tick();
                    InvalidateRect(hWnd_, nullptr, FALSE);
                } else {
                    Sleep(1);
                }
            }
        }

        audioDevice_.stop();
        return static_cast<int>(msg.wParam);
    }

private:
    static LRESULT CALLBACK wndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        TBD16EmulatorApp* app = nullptr;
        if (uMsg == WM_CREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = reinterpret_cast<TBD16EmulatorApp*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        } else {
            app = reinterpret_cast<TBD16EmulatorApp*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        }

        if (app) {
            return app->wndProc(hWnd, uMsg, wParam, lParam);
        }
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    LRESULT wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hWnd, &ps);
                renderChassis();
                
                BITMAPINFO bmi{};
                bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth       = WIN_WIDTH;
                bmi.bmiHeader.biHeight      = -WIN_HEIGHT; // Top-down
                bmi.bmiHeader.biPlanes      = 1;
                bmi.bmiHeader.biBitCount    = 32;
                bmi.bmiHeader.biCompression = BI_RGB;

                StretchDIBits(hdc, 0, 0, WIN_WIDTH, WIN_HEIGHT,
                              0, 0, WIN_WIDTH, WIN_HEIGHT,
                              framebuffer_.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);

                EndPaint(hWnd, &ps);
                return 0;
            }

            case WM_KEYDOWN: {
                handleKeyDown(static_cast<uint32_t>(wParam));
                return 0;
            }

            case WM_LBUTTONDOWN: {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                handleMouseDown(x, y);
                return 0;
            }

            case WM_LBUTTONUP: {
                handleMouseUp();
                return 0;
            }

            case WM_MOUSEMOVE: {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                handleMouseMove(x, y, (wParam & MK_LBUTTON) != 0);
                return 0;
            }

            case WM_MOUSEWHEEL: {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int steps = delta / WHEEL_DELTA;
                if (steps != 0) {
                    InputEvent ev{};
                    ev.input = (steps > 0) ? INPUT_JOG_CW : INPUT_JOG_CCW;
                    ev.pressed = true;
                    ev.jog_delta = static_cast<int16_t>(steps);
                    dispatchInput(ev);
                }
                return 0;
            }

            case WM_DESTROY: {
                PostQuitMessage(0);
                return 0;
            }
        }
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    void setupControls() {
        // Encoders (Y = 535)
        int knobY = 552;
        knobs_[0] = {  65, knobY, 20, "ENC 1 (ROW)", 0.0f, 0 };
        knobs_[1] = { 185, knobY, 20, "ENC 2 (COL)", 0.0f, 1 };
        knobs_[2] = { 335, knobY, 20, "ENC 3 (VAL)", 0.0f, 2 };
        knobs_[3] = { 455, knobY, 20, "ENC 4 (CRS)", 0.0f, 3 };

        // Tactical & Nav Buttons (Y = 590 to 650)
        // Left Nav Cluster: Screen L, Screen R, D-Pad
        buttons_.push_back({  25, 595, 45, 24, "SCR L", INPUT_PAGE_PREV, false, 0xFF4488FF });
        buttons_.push_back({  80, 595, 45, 24, "SCR R", INPUT_PAGE_NEXT, false, 0xFF4488FF });

        buttons_.push_back({  60, 625, 30, 22, "^", INPUT_UP, false, COL_BTN_BORDER });
        buttons_.push_back({  25, 642, 30, 22, "<", INPUT_LEFT, false, COL_BTN_BORDER });
        buttons_.push_back({  60, 650, 30, 22, "v", INPUT_DOWN, false, COL_BTN_BORDER });
        buttons_.push_back({  95, 642, 30, 22, ">", INPUT_RIGHT, false, COL_BTN_BORDER });

        // Transport & Action Cluster: PLAY, REC, A, B, X, Y
        buttons_.push_back({ 145, 600, 55, 28, "PLAY", INPUT_TRANSPORT, false, 0xFF00FF88 });
        buttons_.push_back({ 145, 638, 55, 28, "REC",  INPUT_RECORD,    false, 0xFFFF3333 });

        buttons_.push_back({ 395, 595, 45, 28, "OPT (X)", INPUT_BTN_OPT, false, 0xFF55FFFF });
        buttons_.push_back({ 450, 595, 45, 28, "EDT (Y)", INPUT_BTN_EDIT, false, 0xFFFFCC00 });
        buttons_.push_back({ 395, 635, 45, 28, "DEC (B)", INPUT_BTN_B, false, 0xFFCC6666 });
        buttons_.push_back({ 450, 635, 45, 28, "INC (A)", INPUT_BTN_A, false, 0xFF66FF88 });

        // 16 Step Buttons (2 rows of 8, Y = 688 and 725)
        stepButtons_.clear();
        for (int s = 0; s < 16; ++s) {
            int row = s / 8;
            int col = s % 8;
            int bx = 25 + col * 59;
            int by = 688 + row * 34;
            char lbl[8];
            std::snprintf(lbl, sizeof(lbl), "%02X", s);
            RectButton btn{ bx, by, 52, 28, "", INPUT_BTN_EDIT, false, 0xFF00FF88 };
            stepButtons_.push_back(btn);
        }
    }

    void handleKeyDown(uint32_t vk) {
        InputEvent ev{};
        ev.pressed = true;

        switch (vk) {
            // D-Pad
            case VK_UP:    ev.input = INPUT_UP; break;
            case VK_DOWN:  ev.input = INPUT_DOWN; break;
            case VK_LEFT:  ev.input = INPUT_LEFT; break;
            case VK_RIGHT: ev.input = INPUT_RIGHT; break;

            // Buttons A / B / X / Y
            case 'Z':
            case VK_RETURN: ev.input = INPUT_BTN_A; break;
            case 'X':
            case VK_ESCAPE: ev.input = INPUT_BTN_B; break;
            case 'C':       ev.input = INPUT_BTN_OPT; break;
            case 'V':
            case VK_TAB:    ev.input = INPUT_BTN_EDIT; break;

            // Screen Page Prev / Next
            case 'A': ev.input = INPUT_PAGE_PREV; break;
            case 'S': ev.input = INPUT_PAGE_NEXT; break;

            // Transport
            case VK_SPACE: ev.input = INPUT_TRANSPORT; break;
            case 'R':      ev.input = INPUT_RECORD; break;

            // Jog Wheel Simulation
            case VK_OEM_4: // '['
                ev.input = INPUT_JOG_CCW; ev.jog_delta = -1; break;
            case VK_OEM_6: // ']'
                ev.input = INPUT_JOG_CW; ev.jog_delta = 1; break;
            case VK_OEM_MINUS: // '-'
                ev.input = INPUT_JOG_CCW; ev.jog_delta = -4; break;
            case VK_OEM_PLUS: // '='
                ev.input = INPUT_JOG_CW; ev.jog_delta = 4; break;

            // Number keys 1..8 for Step Selection / Audition
            case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': {
                int step = static_cast<int>(vk - '1');
                uiState_.cursor_row = step;
                uiState_.selected_phrase_step = static_cast<uint8_t>(step);
                ev.input = INPUT_BTN_EDIT;
                break;
            }

            default: return;
        }

        dispatchInput(ev);
    }

    void handleMouseDown(int x, int y) {
        for (auto& btn : buttons_) {
            if (x >= btn.x && x < btn.x + btn.w && y >= btn.y && y < btn.y + btn.h) {
                btn.isPressed = true;
                InputEvent ev{};
                ev.input = btn.input;
                ev.pressed = true;
                dispatchInput(ev);
                return;
            }
        }

        for (size_t s = 0; s < stepButtons_.size(); ++s) {
            auto& btn = stepButtons_[s];
            if (x >= btn.x && x < btn.x + btn.w && y >= btn.y && y < btn.y + btn.h) {
                btn.isPressed = true;
                uiState_.cursor_row = static_cast<int>(s);
                uiState_.selected_phrase_step = static_cast<uint8_t>(s);
                InputEvent ev{};
                ev.input = INPUT_BTN_EDIT;
                ev.pressed = true;
                dispatchInput(ev);
                return;
            }
        }

        // Check Knobs
        for (int k = 0; k < 4; ++k) {
            int dx = x - knobs_[k].cx;
            int dy = y - knobs_[k].cy;
            if (dx * dx + dy * dy <= knobs_[k].radius * knobs_[k].radius) {
                activeKnob_ = k;
                dragStartY_ = y;
                return;
            }
        }
    }

    void handleMouseUp() {
        for (auto& btn : buttons_) btn.isPressed = false;
        for (auto& btn : stepButtons_) btn.isPressed = false;
        activeKnob_ = -1;
    }

    void handleMouseMove(int x, int y, bool isLeftDown) {
        (void)x;
        if (isLeftDown && activeKnob_ >= 0 && activeKnob_ < 4) {
            int dy = dragStartY_ - y;
            if (std::abs(dy) >= 4) {
                int delta = (dy > 0) ? 1 : -1;
                dragStartY_ = y;
                knobs_[activeKnob_].angle += delta * 0.2f;

                InputEvent ev{};
                ev.pressed = true;
                if (activeKnob_ == 0) {
                    ev.input = (delta > 0) ? INPUT_UP : INPUT_DOWN;
                } else if (activeKnob_ == 1) {
                    ev.input = (delta > 0) ? INPUT_RIGHT : INPUT_LEFT;
                } else if (activeKnob_ == 2) {
                    ev.input = (delta > 0) ? INPUT_JOG_CW : INPUT_JOG_CCW;
                    ev.jog_delta = static_cast<int16_t>(delta);
                } else if (activeKnob_ == 3) {
                    ev.input = (delta > 0) ? INPUT_JOG_CW : INPUT_JOG_CCW;
                    ev.jog_delta = static_cast<int16_t>(delta * 8);
                }
                dispatchInput(ev);
            }
        }
    }

    void dispatchInput(const InputEvent& ev) {
        uiState_.handleInput(ev, song_, engine_);
        engine_.loadSong(song_); // Sync modified song structure
    }

    void tick() {
        // Poll Gamepad (XInput)
        gamepad_.poll([this](const InputEvent& ev) {
            dispatchInput(ev);
        });

        // Update NeoPixel Step LEDs via ESP32HAL
        uint8_t playingStep = engine_.getTrackState(0).phrase_step;
        uint8_t activeStep = static_cast<uint8_t>(uiState_.cursor_row);
        esp32HAL_.updateStepLeds(activeStep, playingStep, engine_.isPlaying());
    }

    void renderChassis() {
        // Clear background
        std::fill(framebuffer_.begin(), framebuffer_.end(), COL_CHASSIS_BG);

        // Draw Bezel
        drawRect(LCD_OFF_X - 6, LCD_OFF_Y - 6, LCD_W + 12, LCD_H + 12, COL_BEZEL);
        drawHollowRect(LCD_OFF_X - 7, LCD_OFF_Y - 7, LCD_W + 14, LCD_H + 14, COL_CHASSIS_RIM);

        // Header Text & Status
        drawMiniText(LCD_OFF_X, 12, "DADAMACHINES TBD-16  [ESP32-P4 CORE 0/1]", 0xFF88AACC);
        drawMiniText(380, 12, engine_.isPlaying() ? "RUNNING [44.1k]" : "STOPPED [IDLE]",
                     engine_.isPlaying() ? 0xFF00FF88 : 0xFFFFCC00);

        // Render Core 240x240 LCD into 480x480 Viewport
        displayEngine_.render(uiState_, song_, engine_);
        const uint32_t* srcArgb = displayEngine_.getArgbBuffer();

        for (int y = 0; y < static_cast<int>(DisplayEngine::SCREEN_HEIGHT); ++y) {
            int dstYBase = LCD_OFF_Y + y * LCD_SCALE;
            for (int sy = 0; sy < LCD_SCALE; ++sy) {
                int dstRow = (dstYBase + sy) * WIN_WIDTH + LCD_OFF_X;
                for (int x = 0; x < static_cast<int>(DisplayEngine::SCREEN_WIDTH); ++x) {
                    uint32_t px = srcArgb[y * DisplayEngine::SCREEN_WIDTH + x];
                    for (int sx = 0; sx < LCD_SCALE; ++sx) {
                        framebuffer_[dstRow + x * LCD_SCALE + sx] = px;
                    }
                }
            }
        }

        // Draw 4 Rotary Encoders
        for (int k = 0; k < 4; ++k) {
            drawKnob(knobs_[k]);
        }

        // Draw Tactical Buttons
        for (const auto& btn : buttons_) {
            drawButton(btn);
        }

        // Draw 16 Step Buttons with NeoPixel RGB glow
        const auto& leds = esp32HAL_.getStepLeds();
        for (size_t s = 0; s < stepButtons_.size(); ++s) {
            const auto& btn = stepButtons_[s];
            const auto& led = leds[s];
            uint32_t ledRgb = 0xFF000000 | (led.r << 16) | (led.g << 8) | led.b;
            drawStepButton(btn, s, ledRgb);
        }
    }

    void drawRect(int x, int y, int w, int h, uint32_t col) {
        int x0 = std::max(0, x), y0 = std::max(0, y);
        int x1 = std::min(WIN_WIDTH, x + w), y1 = std::min(WIN_HEIGHT, y + h);
        for (int cy = y0; cy < y1; ++cy) {
            int row = cy * WIN_WIDTH;
            for (int cx = x0; cx < x1; ++cx) {
                framebuffer_[row + cx] = col;
            }
        }
    }

    void drawHollowRect(int x, int y, int w, int h, uint32_t col) {
        drawRect(x, y, w, 1, col);
        drawRect(x, y + h - 1, w, 1, col);
        drawRect(x, y, 1, h, col);
        drawRect(x + w - 1, y, 1, h, col);
    }

    void drawButton(const RectButton& btn) {
        uint32_t bg = btn.isPressed ? btn.accentCol : COL_BTN_FACE;
        drawRect(btn.x, btn.y, btn.w, btn.h, bg);
        drawHollowRect(btn.x, btn.y, btn.w, btn.h, btn.accentCol);
        drawMiniText(btn.x + 4, btn.y + (btn.h / 2) - 4, btn.label, btn.isPressed ? 0xFF000000 : COL_BTN_TEXT);
    }

    void drawStepButton(const RectButton& btn, size_t index, uint32_t ledColor) {
        uint32_t bg = btn.isPressed ? 0xFFFFFFFF : COL_BTN_FACE;
        drawRect(btn.x, btn.y, btn.w, btn.h, bg);

        // NeoPixel LED Bar across top of button
        drawRect(btn.x + 2, btn.y + 2, btn.w - 4, 4, ledColor);

        // Subtle step index number
        char lbl[8];
        std::snprintf(lbl, sizeof(lbl), "%02X", static_cast<unsigned int>(index));
        drawMiniText(btn.x + 18, btn.y + 12, lbl, (index % 4 == 0) ? 0xFF55FFFF : COL_BTN_TEXT);
        drawHollowRect(btn.x, btn.y, btn.w, btn.h, (index % 4 == 0) ? 0xFF3D444D : COL_BTN_BORDER);
    }

    void drawKnob(const KnobControl& knob) {
        int r = knob.radius;
        for (int dy = -r; dy <= r; ++dy) {
            int cy = knob.cy + dy;
            if (cy < 0 || cy >= WIN_HEIGHT) continue;
            for (int dx = -r; dx <= r; ++dx) {
                int cx = knob.cx + dx;
                if (cx < 0 || cx >= WIN_WIDTH) continue;
                int distSq = dx * dx + dy * dy;
                if (distSq <= r * r) {
                    uint32_t col = (distSq >= (r - 2) * (r - 2)) ? COL_KNOB_RIM : COL_KNOB_BODY;
                    framebuffer_[cy * WIN_WIDTH + cx] = col;
                }
            }
        }

        // Indicator Line
        int lx = knob.cx + static_cast<int>(std::sin(knob.angle) * (r - 4));
        int ly = knob.cy - static_cast<int>(std::cos(knob.angle) * (r - 4));
        drawRect(lx - 1, ly - 1, 3, 3, COL_KNOB_LINE);

        // Label below
        drawMiniText(knob.cx - 28, knob.cy + r + 4, knob.label, 0xFF88AACC);
    }

    void drawMiniText(int x, int y, const char* str, uint32_t col) {
        while (*str) {
            char c = *str++;
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            // Simplified 3x5 font rendering
            for (int r = 0; r < 5; ++r) {
                int cy = y + r;
                if (cy < 0 || cy >= WIN_HEIGHT) continue;
                for (int colIdx = 0; colIdx < 3; ++colIdx) {
                    int cx = x + colIdx;
                    if (cx >= 0 && cx < WIN_WIDTH) {
                        if (c != ' ') {
                            framebuffer_[cy * WIN_WIDTH + cx] = col;
                        }
                    }
                }
            }
            x += 4;
        }
    }

    HWND hWnd_{nullptr};
    std::vector<uint32_t> framebuffer_{};

    Engine engine_{};
    Song song_{};
    UIState uiState_{};
    DisplayEngine displayEngine_{};
    ESP32HAL esp32HAL_{};

    Win32AudioDevice audioDevice_{};
    Win32Gamepad gamepad_{};

    std::vector<RectButton> buttons_{};
    std::vector<RectButton> stepButtons_{};
    KnobControl knobs_[4]{};
    int activeKnob_{-1};
    int dragStartY_{0};
};

} // namespace toad

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    auto app = std::make_unique<toad::TBD16EmulatorApp>();
    if (!app->initialize(hInstance, nCmdShow)) {
        MessageBoxW(nullptr, L"Failed to initialize TBD-16 Emulator", L"ToadTracker Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    return app->run();
}
