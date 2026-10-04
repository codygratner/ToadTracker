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
#include <toad/serializer.h>
#include <toad/auto_sampler.h>
#include <toad/keymap.h>

#include "../common/demo_song.h"
#include "../common/win32_audio.h"
#include "../common/win32_gamepad.h"

#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace toad {

// ============================================================================
// DESKTOP TRACKER CONSTANTS & RESOLUTION MODES
// ============================================================================
constexpr int CANVAS_NATIVE_W = DisplayEngine::SCREEN_WIDTH;  // 240
constexpr int CANVAS_NATIVE_H = DisplayEngine::SCREEN_HEIGHT; // 240

constexpr int TOP_BAR_H    = 0;
constexpr int BOTTOM_BAR_H = 28;

// Desktop Color Palette
constexpr uint32_t COL_DESKTOP_BG    = 0xFF080C0A;
constexpr uint32_t COL_BAR_BG        = 0xFF101614;
constexpr uint32_t COL_BAR_BORDER    = 0xFF182A20;
constexpr uint32_t COL_TAB_ACTIVE    = 0xFF00FF88;
constexpr uint32_t COL_TAB_INACTIVE  = 0xFF446655;
constexpr uint32_t COL_TAB_TEXT_ACT  = 0xFF000000;
constexpr uint32_t COL_TAB_TEXT_INAC = 0xFFAABBCC;
constexpr uint32_t COL_TEXT_EMERALD  = 0xFF33FF66;
constexpr uint32_t COL_TEXT_CYAN     = 0xFF55FFFF;
constexpr uint32_t COL_TEXT_AMBER    = 0xFFFFCC00;
constexpr uint32_t COL_TEXT_ALERT    = 0xFFFF3333;
constexpr uint32_t COL_TEXT_DIM      = 0xFF557766;

class DesktopTrackerApp {
public:
    DesktopTrackerApp() {
        scale_ = 3; // Default 3x (720x720 canvas)
        updateWindowSize();
        if (!inputMap_.loadFromFile("toad_settings.dat")) {
            inputMap_.loadM8RunPreset();
        }
    }

    bool initialize(HINSTANCE hInstance, int nCmdShow) {
        hInstance_ = hInstance;

        // Initialize Core Engine
        engine_.setSampleRate(44100.0f);
        engine_.setBpm(128.0f);
        song_ = createDemoSong();
        engine_.loadSong(song_);

        // Register Desktop Window Class
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = wndProcStatic;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"ToadTrackerDesktopClass";

        if (!RegisterClassExW(&wc)) return false;

        RECT rc = { 0, 0, winWidth_, winHeight_ };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, FALSE);

        hWnd_ = CreateWindowExW(
            0,
            L"ToadTrackerDesktopClass",
            L"ToadTracker (\"The Toad\") - Standalone Desktop Tracker v0.2.0",
            (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME),
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, hInstance, this
        );

        if (!hWnd_) return false;

        // Start Low-Latency Audio Device (with safe fallback if no audio device)
        bool audioOk = audioDevice_.start(44100, [this](float* stereoBuffer, size_t frames) {
            engine_.renderBlockDeterministic(stereoBuffer, frames);

            // Update VU Levels
            for (size_t t = 0; t < MAX_TRACKS; ++t) {
                const auto& trk = engine_.getTrackState(t);
                float lvl = trk.voice_active ? (static_cast<float>(trk.effective_volume) / 255.0f) : 0.0f;
                uiState_.vu_levels[t] = std::max(lvl, uiState_.vu_levels[t] * 0.90f);
            }
        });

        audioAvailable_ = audioOk;

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
                if (now - lastTick >= 16) { // ~60 FPS
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
    void updateWindowSize() {
        winWidth_ = CANVAS_NATIVE_W * scale_;
        winHeight_ = CANVAS_NATIVE_H * scale_ + TOP_BAR_H + BOTTOM_BAR_H;
        framebuffer_.assign(winWidth_ * winHeight_, COL_DESKTOP_BG);
    }

    static LRESULT CALLBACK wndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        DesktopTrackerApp* app = nullptr;
        if (uMsg == WM_CREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = reinterpret_cast<DesktopTrackerApp*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        } else {
            app = reinterpret_cast<DesktopTrackerApp*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
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
                renderAll();

                BITMAPINFO bmi{};
                bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth       = winWidth_;
                bmi.bmiHeader.biHeight      = -winHeight_; // Top-down
                bmi.bmiHeader.biPlanes      = 1;
                bmi.bmiHeader.biBitCount    = 32;
                bmi.bmiHeader.biCompression = BI_RGB;

                StretchDIBits(hdc, 0, 0, winWidth_, winHeight_,
                              0, 0, winWidth_, winHeight_,
                              framebuffer_.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);

                EndPaint(hWnd, &ps);
                return 0;
            }

            case WM_KEYDOWN: {
                handleKeyDown(static_cast<uint32_t>(wParam), (GetKeyState(VK_CONTROL) & 0x8000) != 0,
                                                              (GetKeyState(VK_SHIFT) & 0x8000) != 0);
                return 0;
            }

            case WM_LBUTTONDOWN: {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                handleMouseDown(x, y);
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

    void handleKeyDown(uint32_t vk, bool ctrl, bool shift) {
        (void)shift;

        // Project File Operations (Ctrl+S, Ctrl+O, Ctrl+B)
        if (ctrl) {
            if (vk == 'S') { // Save Song
                bool ok = Serializer::saveSong("song.tts", song_);
                setStatusMessage(ok ? "SAVED: song.tts" : "ERROR SAVING SONG");
                return;
            } else if (vk == 'O') { // Open Song
                bool ok = Serializer::loadSong("song.tts", song_);
                if (ok) {
                    engine_.loadSong(song_);
                    setStatusMessage("LOADED: song.tts");
                } else {
                    setStatusMessage("ERROR: Could not open song.tts");
                }
                return;
            } else if (vk == 'B') { // Bake Instrument Note to WAV
                auto baked = AutoSampler::bakeNote(engine_, uiState_.selected_instrument_id, 60, 1.0f, 0.25f, 44100, true);
                auto wav = AutoSampler::createWavFile(baked);
                FILE* f = fopen("baked_sample.wav", "wb");
                if (f) {
                    fwrite(wav.data(), 1, wav.size(), f);
                    fclose(f);
                    setStatusMessage("BAKED: baked_sample.wav");
                } else {
                    setStatusMessage("ERROR WRITING WAV");
                }
                return;
            } else if (vk == '1') { // Scale 2x
                setScale(2); return;
            } else if (vk == '2') { // Scale 3x
                setScale(3); return;
            } else if (vk == '3') { // Scale 4x
                setScale(4); return;
            }
        }

        // Direct View Hotkeys (F1..F8)
        if (vk >= VK_F1 && vk <= VK_F8) {
            TrackerView target = static_cast<TrackerView>(vk - VK_F1);
            if (target < VIEW_COUNT) {
                uiState_.current_view = target;
                uiState_.cursor_row = 0;
                uiState_.cursor_col = 0;
            }
            return;
        }

        // Scale Toggle (F12)
        if (vk == VK_F12) {
            int nextScale = (scale_ == 3) ? 4 : ((scale_ == 4) ? 2 : 3);
            setScale(nextScale);
            return;
        }

        // --- 0. 2D SCREEN NAVIGATION MAP (Shift + Arrows) ---
        if (shift) {
            if (vk == VK_UP) {
                uiState_.navigate2D(0, -1, song_);
                return;
            } else if (vk == VK_DOWN) {
                uiState_.navigate2D(0, 1, song_);
                return;
            } else if (vk == VK_LEFT) {
                uiState_.navigate2D(-1, 0, song_);
                return;
            } else if (vk == VK_RIGHT) {
                uiState_.navigate2D(1, 0, song_);
                return;
            }
        }

        // --- 1. SETTINGS VIEW LOGIC & REMAPPING ---
        if (uiState_.current_view == VIEW_SETTINGS) {
            if (uiState_.is_remapping) {
                if (vk == VK_ESCAPE) {
                    uiState_.is_remapping = false;
                    setStatusMessage("REMAP CANCELLED");
                    return;
                }
                inputMap_.remapPrimaryKey(uiState_.remap_action_index, vk);
                inputMap_.saveToFile("toad_settings.dat");
                uiState_.is_remapping = false;
                setStatusMessage("KEY REBOUND & SAVED");
                return;
            }

            // Quick Preset Selection (Keys 1..4 on Settings Page)
            if (vk == '1') {
                inputMap_.loadM8RunPreset();
                inputMap_.saveToFile("toad_settings.dat");
                setStatusMessage("PRESET: M8.RUN (A/S:PAGE L/R, Z/X:EDIT/BACK)");
                return;
            } else if (vk == '2') {
                inputMap_.loadDesktopPreset();
                inputMap_.saveToFile("toad_settings.dat");
                setStatusMessage("PRESET: DESKTOP (ARROWS/ENTER/ESC)");
                return;
            } else if (vk == '3') {
                inputMap_.loadWasdPreset();
                inputMap_.saveToFile("toad_settings.dat");
                setStatusMessage("PRESET: WASD (WASD/JK/SPACE)");
                return;
            } else if (vk == '4') {
                inputMap_.loadVimPreset();
                inputMap_.saveToFile("toad_settings.dat");
                setStatusMessage("PRESET: VIM (HJKL/ZX/SPACE)");
                return;
            }

            // Start Remap on Enter or primary action button
            if (vk == VK_RETURN || inputMap_.resolveKey(vk) == INPUT_BTN_A) {
                uiState_.is_remapping = true;
                uiState_.remap_action_index = uiState_.cursor_row;
                setStatusMessage("PRESS ANY KEY TO BIND (ESC:CANCEL)");
                return;
            }
        }

        // --- 2. OCTAVE SHIFTING (',' / '<' and '.' / '>') ---
        if (InputMap::isOctaveDownKey(vk)) {
            if (uiState_.octave_offset > -4) {
                uiState_.octave_offset--;
                setStatusMessage("OCTAVE DOWN");
            }
            return;
        }
        if (InputMap::isOctaveUpKey(vk)) {
            if (uiState_.octave_offset < 4) {
                uiState_.octave_offset++;
                setStatusMessage("OCTAVE UP");
            }
            return;
        }

        // --- 3. VIRTUAL PIANO KEYBOARD (Q..], 2..=) ---
        int pianoNote = InputMap::resolveVirtualPianoNote(vk, uiState_.octave_offset);
        if (pianoNote >= 0) {
            if (uiState_.current_view == VIEW_PHRASE && uiState_.cursor_col == 0) {
                auto& ph = song_.phrases[uiState_.selected_phrase_id < TOTAL_PHRASES ? uiState_.selected_phrase_id : 0];
                auto& step = ph.steps[uiState_.cursor_row < 16 ? uiState_.cursor_row : 0];
                step.note = static_cast<uint8_t>(pianoNote);
                step.instrument = uiState_.selected_instrument_id;
                if (step.volume == 0) step.volume = 0xFF;

                // Audition Note immediately
                engine_.getTrackState(uiState_.active_track).raw_note = step.note;
                engine_.getTrackState(uiState_.active_track).effective_note = static_cast<int8_t>(step.note);
                engine_.loadSong(song_);

                // Auto-advance cursor step
                if (uiState_.cursor_row < 15) {
                    uiState_.cursor_row++;
                    uiState_.selected_phrase_step = static_cast<uint8_t>(uiState_.cursor_row);
                }
            } else {
                // Live Audition on Active Track in all views
                engine_.getTrackState(uiState_.active_track).raw_note = static_cast<uint8_t>(pianoNote);
                engine_.getTrackState(uiState_.active_track).effective_note = static_cast<int8_t>(pianoNote);
                engine_.loadSong(song_);
            }
            return;
        }

        // --- 4. CONFIGURABLE INPUT MAPPING TABLE ---
        LogicalInput mappedAction = inputMap_.resolveKey(vk);
        if (mappedAction != INPUT_NONE) {
            InputEvent ev{};
            ev.input = mappedAction;
            ev.pressed = true;
            dispatchInput(ev);
            return;
        }

        // --- 5. NUMERIC / HEX COLUMN INPUT FALLBACK ---
        // Used in Song, Chain, or Phrase (Inst, Vol, FX columns)
        if (vk >= '0' && vk <= '9') {
            applyHexInput(static_cast<int>(vk - '0'));
            return;
        }
        if (vk >= 'A' && vk <= 'F') {
            applyHexInput(static_cast<int>(10 + (vk - 'A')));
            return;
        }
    }

    void applyHexInput(int hexDigit) {
        if (uiState_.current_view == VIEW_SONG) {
            uint8_t& chain = song_.rows[uiState_.cursor_row].chain_ids[uiState_.cursor_col];
            chain = (chain == CHAIN_EMPTY) ? static_cast<uint8_t>(hexDigit) : static_cast<uint8_t>(((chain << 4) | hexDigit) & 0xFF);
        } else if (uiState_.current_view == VIEW_CHAIN) {
            auto& ch = song_.chains[uiState_.selected_chain_id < TOTAL_CHAINS ? uiState_.selected_chain_id : 0];
            if (uiState_.cursor_col == 0) {
                uint8_t& ph = ch.steps[uiState_.cursor_row].phrase_id;
                ph = (ph == PHRASE_EMPTY) ? static_cast<uint8_t>(hexDigit) : static_cast<uint8_t>(((ph << 4) | hexDigit) & 0xFF);
            }
        } else if (uiState_.current_view == VIEW_PHRASE) {
            auto& ph = song_.phrases[uiState_.selected_phrase_id < TOTAL_PHRASES ? uiState_.selected_phrase_id : 0];
            auto& step = ph.steps[uiState_.cursor_row];
            if (uiState_.cursor_col == 1) { // Inst
                step.instrument = static_cast<uint8_t>(((step.instrument << 4) | hexDigit) % TOTAL_INSTRUMENTS);
            } else if (uiState_.cursor_col == 2) { // Vol
                step.volume = static_cast<uint8_t>((step.volume << 4) | hexDigit);
            } else if (uiState_.cursor_col == 4 || uiState_.cursor_col == 6 || uiState_.cursor_col == 8) { // FX Val
                int fxIdx = (uiState_.cursor_col - 4) / 2;
                step.fx[fxIdx].val = static_cast<uint8_t>((step.fx[fxIdx].val << 4) | hexDigit);
            }
        }
        engine_.loadSong(song_);
    }

    void handleMouseDown(int x, int y) {
        // Mini-Map Click (Top-Right Corner: X >= 210 * scale, Y <= 14 * scale)
        if (x >= 210 * scale_ && x <= winWidth_ && y <= 14 * scale_) {
            uiState_.nav_hud_timer = (uiState_.nav_hud_timer > 0) ? 0 : 120; // Toggle HUD
        }
    }

    void setScale(int s) {
        if (s < 2 || s > 4) return;
        scale_ = s;
        updateWindowSize();

        RECT rc = { 0, 0, winWidth_, winHeight_ };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, FALSE);
        SetWindowPos(hWnd_, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
    }

    void setStatusMessage(const char* msg) {
        statusMsg_ = msg;
        statusMsgTimer_ = 180; // 3 seconds @ 60 FPS
    }

    void dispatchInput(const InputEvent& ev) {
        uiState_.handleInput(ev, song_, engine_);
        engine_.loadSong(song_);
    }

    void tick() {
        uiState_.tickNavHUD();

        gamepad_.poll([this](const InputEvent& ev) {
            if (ev.modifier_view && ev.pressed) {
                if (ev.input == INPUT_UP)    { uiState_.navigate2D(0, -1, song_); return; }
                if (ev.input == INPUT_DOWN)  { uiState_.navigate2D(0,  1, song_); return; }
                if (ev.input == INPUT_LEFT)  { uiState_.navigate2D(-1, 0, song_); return; }
                if (ev.input == INPUT_RIGHT) { uiState_.navigate2D( 1, 0, song_); return; }
            }
            dispatchInput(ev);
        });

        if (statusMsgTimer_ > 0) {
            statusMsgTimer_--;
        }
    }

    void renderAll() {
        // 1. Render Core 240x240 Tracker Canvas Scaled to Window (Starts at Y=0)
        displayEngine_.render(uiState_, song_, engine_, inputMap_);
        const uint32_t* srcArgb = displayEngine_.getArgbBuffer();

        int canvasY = 0;
        for (int y = 0; y < CANVAS_NATIVE_H; ++y) {
            int dstYBase = canvasY + y * scale_;
            for (int sy = 0; sy < scale_; ++sy) {
                int dstRow = (dstYBase + sy) * winWidth_;
                for (int x = 0; x < CANVAS_NATIVE_W; ++x) {
                    uint32_t px = srcArgb[y * CANVAS_NATIVE_W + x];
                    for (int sx = 0; sx < scale_; ++sx) {
                        framebuffer_[dstRow + x * scale_ + sx] = px;
                    }
                }
            }
        }

        // 2. Render Bottom Desktop Status Bar
        renderBottomBar();
    }

    void renderBottomBar() {
        int by = winHeight_ - BOTTOM_BAR_H;
        drawRect(0, by, winWidth_, BOTTOM_BAR_H, COL_BAR_BG);
        drawHLine(0, by, winWidth_, COL_BAR_BORDER);

        // Status or Toast Message
        char leftInfo[128];
        if (statusMsgTimer_ > 0) {
            drawText(12, by + 8, statusMsg_.c_str(), COL_TEXT_CYAN);
        } else {
            std::snprintf(leftInfo, sizeof(leftInfo), "%s R%02X %03dBPM | OCT%+d | T%d | %s",
                          engine_.isPlaying() ? "> PLAYING" : "[] STOPPED",
                          engine_.getSongRow(),
                          static_cast<int>(song_.bpm),
                          uiState_.octave_offset,
                          uiState_.active_track,
                          audioAvailable_ ? "44.1k AUDIO OK" : "SILENT MODE");
            drawText(12, by + 8, leftInfo, engine_.isPlaying() ? COL_TEXT_EMERALD : COL_TEXT_AMBER);
        }

        // Right side: Quick Shortcuts Hint
        char rightHint[64];
        std::snprintf(rightHint, sizeof(rightHint), "Scale: %dx [F12] | Ctrl+S:Save", scale_);
        drawText(winWidth_ - 260, by + 8, rightHint, COL_TEXT_DIM);
    }

    void drawRect(int x, int y, int w, int h, uint32_t col) {
        int x0 = std::max(0, x), y0 = std::max(0, y);
        int x1 = std::min(winWidth_, x + w), y1 = std::min(winHeight_, y + h);
        for (int cy = y0; cy < y1; ++cy) {
            int row = cy * winWidth_;
            for (int cx = x0; cx < x1; ++cx) {
                framebuffer_[row + cx] = col;
            }
        }
    }

    void drawHLine(int x, int y, int w, uint32_t col) {
        drawRect(x, y, w, 1, col);
    }

    void drawHollowRect(int x, int y, int w, int h, uint32_t col) {
        drawRect(x, y, w, 1, col);
        drawRect(x, y + h - 1, w, 1, col);
        drawRect(x, y, 1, h, col);
        drawRect(x + w - 1, y, 1, h, col);
    }

    void drawText(int x, int y, const char* str, uint32_t col) {
        while (*str) {
            char c = *str++;
            for (int r = 0; r < 6; ++r) {
                int cy = y + r;
                if (cy < 0 || cy >= winHeight_) continue;
                for (int colIdx = 0; colIdx < 4; ++colIdx) {
                    int cx = x + colIdx;
                    if (cx >= 0 && cx < winWidth_) {
                        if (c != ' ') {
                            framebuffer_[cy * winWidth_ + cx] = col;
                        }
                    }
                }
            }
            x += 6;
        }
    }

    HINSTANCE hInstance_{nullptr};
    HWND hWnd_{nullptr};

    int scale_{3};
    int winWidth_{720};
    int winHeight_{776};

    std::vector<uint32_t> framebuffer_{};

    Engine engine_{};
    Song song_{};
    UIState uiState_{};
    DisplayEngine displayEngine_{};
    InputMap inputMap_{};

    Win32AudioDevice audioDevice_{};
    Win32Gamepad gamepad_{};
    bool audioAvailable_{false};

    std::string statusMsg_{};
    int statusMsgTimer_{0};
};

} // namespace toad

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    auto app = std::make_unique<toad::DesktopTrackerApp>();
    if (!app->initialize(hInstance, nCmdShow)) {
        MessageBoxW(nullptr, L"Failed to initialize ToadTracker Desktop", L"ToadTracker Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    return app->run();
}
