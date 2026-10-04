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

constexpr int TOP_BAR_H    = 28;
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

        // Direct View Hotkeys (F1..F7)
        if (vk >= VK_F1 && vk <= VK_F7) {
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

        // Standard Navigation Keys
        InputEvent ev{};
        ev.pressed = true;

        switch (vk) {
            case VK_UP:    ev.input = INPUT_UP; break;
            case VK_DOWN:  ev.input = INPUT_DOWN; break;
            case VK_LEFT:  ev.input = INPUT_LEFT; break;
            case VK_RIGHT: ev.input = INPUT_RIGHT; break;

            case VK_PRIOR: // Page Up -> Prev View
                ev.input = INPUT_PAGE_PREV; break;
            case VK_NEXT:  // Page Down -> Next View
                ev.input = INPUT_PAGE_NEXT; break;

            case VK_RETURN: // Enter -> Sub-View / Confirm / Inc
                ev.input = INPUT_BTN_A; break;
            case VK_BACK:   // Backspace -> Delete Note or Back View
            case VK_ESCAPE: // Esc -> Back View
                ev.input = INPUT_BTN_B; break;

            case VK_SPACE: // Space -> Transport Play/Stop
                ev.input = INPUT_TRANSPORT; break;

            case VK_TAB:   // Tab -> Audition Note
                ev.input = INPUT_BTN_EDIT; break;

            case VK_OEM_4: // '[' -> Value dec
                ev.input = INPUT_JOG_CCW; ev.jog_delta = -1; break;
            case VK_OEM_6: // ']' -> Value inc
                ev.input = INPUT_JOG_CW;  ev.jog_delta = 1;  break;

            case VK_OEM_MINUS: // '-' -> Octave Down
                if (uiState_.octave_offset > -4) uiState_.octave_offset--;
                return;
            case VK_OEM_PLUS:  // '=' -> Octave Up
                if (uiState_.octave_offset < 4) uiState_.octave_offset++;
                return;

            default:
                // Check Musical Typing Keyboard (Note Entry)
                if (uiState_.current_view == VIEW_PHRASE) {
                    int note = getMusicalTypingNote(vk);
                    if (note >= 0) {
                        int effectiveNote = note + (uiState_.octave_offset * 12);
                        effectiveNote = std::max(0, std::min(127, effectiveNote));

                        auto& ph = song_.phrases[uiState_.selected_phrase_id < TOTAL_PHRASES ? uiState_.selected_phrase_id : 0];
                        auto& step = ph.steps[uiState_.cursor_row < 16 ? uiState_.cursor_row : 0];
                        step.note = static_cast<uint8_t>(effectiveNote);
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
                        return;
                    }
                }

                // Check Hex Value Input (0..9, A..F) for Numeric columns
                if (vk >= '0' && vk <= '9') {
                    int hexDigit = vk - '0';
                    applyHexInput(hexDigit);
                    return;
                }
                if (vk >= 'A' && vk <= 'F') {
                    int hexDigit = 10 + (vk - 'A');
                    applyHexInput(hexDigit);
                    return;
                }
                return;
        }

        dispatchInput(ev);
    }

    int getMusicalTypingNote(uint32_t vk) {
        // Base octave 4 (Middle C = 60)
        switch (vk) {
            // Lower octave (C-4 .. B-4)
            case 'Z': return 60; // C-4
            case 'S': return 61; // C#4
            case 'X': return 62; // D-4
            case 'D': return 63; // D#4
            case 'C': return 64; // E-4
            case 'V': return 65; // F-4
            case 'G': return 66; // F#4
            case 'B': return 67; // G-4
            case 'H': return 68; // G#4
            case 'N': return 69; // A-4
            case 'J': return 70; // A#4
            case 'M': return 71; // B-4

            // Upper octave (C-5 .. B-5)
            case 'Q': return 72; // C-5
            case '2': return 73; // C#5
            case 'W': return 74; // D-5
            case '3': return 75; // D#5
            case 'E': return 76; // E-5
            case 'R': return 77; // F-5
            case '5': return 78; // F#5
            case 'T': return 79; // G-5
            case '6': return 80; // G#5
            case 'Y': return 81; // A-5
            case '7': return 82; // A#5
            case 'U': return 83; // B-5

            default: return -1;
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
        // Tab Bar Clicks (Y: 0..TOP_BAR_H)
        if (y < TOP_BAR_H) {
            const int tabW = winWidth_ / VIEW_COUNT;
            int tabIdx = x / tabW;
            if (tabIdx >= 0 && tabIdx < VIEW_COUNT) {
                uiState_.current_view = static_cast<TrackerView>(tabIdx);
                uiState_.cursor_row = 0;
                uiState_.cursor_col = 0;
            }
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
        gamepad_.poll([this](const InputEvent& ev) {
            dispatchInput(ev);
        });

        if (statusMsgTimer_ > 0) {
            statusMsgTimer_--;
        }
    }

    void renderAll() {
        // 1. Render Top Desktop View Tab Bar
        renderTopBar();

        // 2. Render Core 240x240 Tracker Canvas Scaled to Window Center
        displayEngine_.render(uiState_, song_, engine_);
        const uint32_t* srcArgb = displayEngine_.getArgbBuffer();

        int canvasY = TOP_BAR_H;
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

        // 3. Render Bottom Desktop Status Bar
        renderBottomBar();
    }

    void renderTopBar() {
        drawRect(0, 0, winWidth_, TOP_BAR_H, COL_BAR_BG);
        drawHLine(0, TOP_BAR_H - 1, winWidth_, COL_BAR_BORDER);

        static const char* viewLabels[] = {
            "F1:SONG", "F2:CHAIN", "F3:PHRASE", "F4:TABLE", "F5:INST", "F6:SYNTH", "F7:PROJ"
        };

        const int tabW = winWidth_ / VIEW_COUNT;
        for (int i = 0; i < VIEW_COUNT; ++i) {
            int tx = i * tabW;
            bool isActive = (uiState_.current_view == i);

            if (isActive) {
                drawRect(tx + 2, 2, tabW - 4, TOP_BAR_H - 4, COL_TAB_ACTIVE);
                drawText(tx + 8, 8, viewLabels[i], COL_TAB_TEXT_ACT);
            } else {
                drawHollowRect(tx + 2, 2, tabW - 4, TOP_BAR_H - 4, COL_TAB_INACTIVE);
                drawText(tx + 8, 8, viewLabels[i], COL_TAB_TEXT_INAC);
            }
        }
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
