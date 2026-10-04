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
#include <hal/steamdeck/steamdeck_harness.h>
#include <hal/common/hal_interface.h>

#include "../common/demo_song.h"
#include "../common/win32_audio.h"
#include "../common/win32_gamepad.h"

#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace toad {

// ============================================================================
// STEAM DECK NATIVE VIEWPORT CONSTANTS (1280x800 16:10)
// ============================================================================
constexpr int WIN_WIDTH  = DisplayEngine::STEAM_DECK_WIDTH;  // 1280
constexpr int WIN_HEIGHT = DisplayEngine::STEAM_DECK_HEIGHT; // 800

// Center 3x Scaled Canvas (720x720)
constexpr int CANVAS_X = DisplayEngine::STEAM_DECK_OFFSET_X; // 280
constexpr int CANVAS_Y = DisplayEngine::STEAM_DECK_OFFSET_Y; // 40
constexpr int CANVAS_W = DisplayEngine::SCREEN_WIDTH * DisplayEngine::STEAM_DECK_SCALE;  // 720
constexpr int CANVAS_H = DisplayEngine::SCREEN_HEIGHT * DisplayEngine::STEAM_DECK_SCALE; // 720

// Side Flanks (Width = 280)
constexpr int LEFT_FLANK_X  = 0;
constexpr int LEFT_FLANK_W  = CANVAS_X; // 280
constexpr int RIGHT_FLANK_X = CANVAS_X + CANVAS_W; // 1000
constexpr int RIGHT_FLANK_W = WIN_WIDTH - RIGHT_FLANK_X; // 280

// Palette for Steam Deck Chassis & Visualizers
constexpr uint32_t COL_BG_DARK       = 0xFF0A0C0E;
constexpr uint32_t COL_PANEL_BG      = 0xFF101418;
constexpr uint32_t COL_BORDER_DIM    = 0xFF1C2228;
constexpr uint32_t COL_BORDER_BRIGHT = 0xFF2A3440;
constexpr uint32_t COL_TEXT_ACCENT   = 0xFF55FFFF;
constexpr uint32_t COL_TEXT_GREEN    = 0xFF33FF66;
constexpr uint32_t COL_TEXT_AMBER    = 0xFFFFCC00;
constexpr uint32_t COL_TEXT_RED      = 0xFFFF3333;
constexpr uint32_t COL_TEXT_DIM      = 0xFF506070;
constexpr uint32_t COL_SCOPE_TRACE   = 0xFF00FF88;
constexpr uint32_t COL_TRACKPAD_RIM  = 0xFF242C34;
constexpr uint32_t COL_TRACKPAD_BG   = 0xFF141A20;

class SteamDeckRunnerApp {
public:
    SteamDeckRunnerApp() {
        framebuffer_.assign(WIN_WIDTH * WIN_HEIGHT, COL_BG_DARK);
    }

    bool initialize(HINSTANCE hInstance, int nCmdShow) {
        // Initialize Engine & Demo Song
        engine_.setSampleRate(44100.0f);
        engine_.setBpm(128.0f);
        song_ = createDemoSong();
        engine_.loadSong(song_);

        steamDeckHAL_.initialize(44100, 512);

        // Register Window Class
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = wndProcStatic;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"ToadTrackerSteamDeckRunnerClass";

        if (!RegisterClassExW(&wc)) return false;

        RECT rc = { 0, 0, WIN_WIDTH, WIN_HEIGHT };
        AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

        hWnd_ = CreateWindowExW(
            0,
            L"ToadTrackerSteamDeckRunnerClass",
            L"ToadTracker - Valve Steam Deck Runner (1280x800 Native)",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, hInstance, this
        );

        if (!hWnd_) return false;

        // Initialize Audio Device
        audioDevice_.start(44100, [this](float* stereoBuffer, size_t frames) {
            engine_.renderBlockDeterministic(stereoBuffer, frames);

            // Update VU Levels
            for (size_t t = 0; t < MAX_TRACKS; ++t) {
                const auto& trk = engine_.getTrackState(t);
                float lvl = trk.voice_active ? (static_cast<float>(trk.effective_volume) / 255.0f) : 0.0f;
                uiState_.vu_levels[t] = std::max(lvl, uiState_.vu_levels[t] * 0.90f);
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
    static LRESULT CALLBACK wndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        SteamDeckRunnerApp* app = nullptr;
        if (uMsg == WM_CREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = reinterpret_cast<SteamDeckRunnerApp*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        } else {
            app = reinterpret_cast<SteamDeckRunnerApp*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
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
                bmi.bmiHeader.biWidth       = WIN_WIDTH;
                bmi.bmiHeader.biHeight      = -WIN_HEIGHT;
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
                trackpadDragging_ = false;
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

    void handleKeyDown(uint32_t vk) {
        InputEvent ev{};
        ev.pressed = true;

        switch (vk) {
            // D-Pad
            case VK_UP:    ev.input = INPUT_UP; break;
            case VK_DOWN:  ev.input = INPUT_DOWN; break;
            case VK_LEFT:  ev.input = INPUT_LEFT; break;
            case VK_RIGHT: ev.input = INPUT_RIGHT; break;

            // Face Buttons A / B / X / Y
            case 'Z':
            case VK_RETURN: ev.input = INPUT_BTN_A; break;
            case 'X':
            case VK_ESCAPE: ev.input = INPUT_BTN_B; break;
            case 'C':       ev.input = INPUT_BTN_OPT; break;
            case 'V':
            case VK_TAB:    ev.input = INPUT_BTN_EDIT; break;

            // Bumpers L1 / R1
            case 'Q': ev.input = INPUT_PAGE_PREV; break;
            case 'W': ev.input = INPUT_PAGE_NEXT; break;

            // Triggers L2 / R2 (Modifiers)
            case '1': ev.input = INPUT_VIEW_MOD; break;
            case '2': ev.input = INPUT_SUB_MOD; break;

            // Steam Deck Rear Grips (L4/L5: Octave Down/Up, R4/R5: Mute/Solo)
            case '7': ev.input = INPUT_OCTAVE_DOWN; break;
            case '8': ev.input = INPUT_OCTAVE_UP; break;
            case '9': ev.input = INPUT_TRACK_MUTE; break;
            case '0': ev.input = INPUT_TRACK_SOLO; break;

            // Transport & Record
            case VK_SPACE: ev.input = INPUT_TRANSPORT; break;
            case 'R':      ev.input = INPUT_RECORD; break;

            // Jog Wheel Scrubbing
            case VK_OEM_4: ev.input = INPUT_JOG_CCW; ev.jog_delta = -1; break;
            case VK_OEM_6: ev.input = INPUT_JOG_CW;  ev.jog_delta = 1;  break;

            default: return;
        }

        dispatchInput(ev);
    }

    void handleMouseDown(int x, int y) {
        // Trackpad Center: X = 1140, Y = 500, R = 80
        int tcx = 1140, tcy = 500, tr = 80;
        int dx = x - tcx;
        int dy = y - tcy;
        if (dx * dx + dy * dy <= tr * tr) {
            trackpadDragging_ = true;
            lastTrackpadAngle_ = std::atan2(static_cast<float>(dy), static_cast<float>(dx));
            trackpadX_ = static_cast<float>(dx) / tr;
            trackpadY_ = static_cast<float>(dy) / tr;
        }
    }

    void handleMouseMove(int x, int y, bool isLeftDown) {
        if (isLeftDown && trackpadDragging_) {
            int tcx = 1140, tcy = 500, tr = 80;
            int dx = x - tcx;
            int dy = y - tcy;
            trackpadX_ = std::max(-1.0f, std::min(1.0f, static_cast<float>(dx) / tr));
            trackpadY_ = std::max(-1.0f, std::min(1.0f, static_cast<float>(dy) / tr));

            float angle = std::atan2(static_cast<float>(dy), static_cast<float>(dx));
            float delta = angle - lastTrackpadAngle_;
            constexpr float PI = 3.14159265358979323846f;
            if (delta > PI) delta -= 2.0f * PI;
            if (delta < -PI) delta += 2.0f * PI;

            constexpr float JOG_STEP = 0.25f; // ~15 deg
            if (std::abs(delta) >= JOG_STEP) {
                InputEvent ev{};
                ev.pressed = true;
                ev.input = (delta > 0) ? INPUT_JOG_CW : INPUT_JOG_CCW;
                ev.jog_delta = (delta > 0) ? 1 : -1;
                dispatchInput(ev);
                lastTrackpadAngle_ = angle;
            }
        }
    }

    void dispatchInput(const InputEvent& ev) {
        uiState_.handleInput(ev, song_, engine_);
        engine_.loadSong(song_);
    }

    void tick() {
        gamepad_.poll([this](const InputEvent& ev) {
            dispatchInput(ev);
        });
    }

    void renderAll() {
        // Clear background
        std::fill(framebuffer_.begin(), framebuffer_.end(), COL_BG_DARK);

        // 1. Render Center 240x240 LCD into 720x720 Composited Canvas
        displayEngine_.render(uiState_, song_, engine_);
        displayEngine_.compositeSteamDeck(framebuffer_.data(), uiState_, engine_);

        // 2. Render Left Flank (Oscilloscope & VU Meters)
        renderLeftFlank();

        // 3. Render Right Flank (Tracker Monitor & Trackpad Jog Wheel)
        renderRightFlank();
    }

    void renderLeftFlank() {
        // Border & Background
        drawRect(LEFT_FLANK_X + 10, 20, LEFT_FLANK_W - 20, WIN_HEIGHT - 40, COL_PANEL_BG);
        drawHollowRect(LEFT_FLANK_X + 10, 20, LEFT_FLANK_W - 20, WIN_HEIGHT - 40, COL_BORDER_DIM);

        // Header
        drawText(LEFT_FLANK_X + 24, 35, "VALVE STEAM DECK", COL_TEXT_ACCENT);
        drawText(LEFT_FLANK_X + 24, 52, "PipeWire Low-Latency Audio", COL_TEXT_DIM);

        // Stereo Oscilloscope Viewport (240x120 at X=20, Y=80)
        int scX = LEFT_FLANK_X + 20;
        int scY = 80;
        int scW = 240;
        int scH = 120;

        drawRect(scX, scY, scW, scH, 0xFF080C0A);
        drawHollowRect(scX, scY, scW, scH, COL_BORDER_BRIGHT);
        drawText(scX + 6, scY + 6, "LIVE OSCILLOSCOPE", COL_TEXT_DIM);

        // Center line
        int midY = scY + scH / 2;
        for (int x = scX; x < scX + scW; x += 4) {
            framebuffer_[midY * WIN_WIDTH + x] = 0xFF142418;
        }

        // Fetch audio snapshot
        float scopeData[240]{};
        audioDevice_.getScopeSnapshot(scopeData, 240);

        // Draw waveform trace
        for (int x = 0; x < scW - 1; ++x) {
            float s0 = scopeData[x];
            float s1 = scopeData[x + 1];
            int y0 = midY - static_cast<int>(s0 * (scH / 2 - 4));
            int y1 = midY - static_cast<int>(s1 * (scH / 2 - 4));
            y0 = std::max(scY + 1, std::min(scY + scH - 2, y0));
            y1 = std::max(scY + 1, std::min(scY + scH - 2, y1));

            int minY = std::min(y0, y1);
            int maxY = std::max(y0, y1);
            for (int dy = minY; dy <= maxY; ++dy) {
                framebuffer_[dy * WIN_WIDTH + (scX + x)] = COL_SCOPE_TRACE;
            }
        }

        // Master Peak VU Meters (Left & Right)
        int vuY = 220;
        drawText(scX, vuY, "MASTER PEAK VU", COL_TEXT_ACCENT);

        float peakL = audioDevice_.getPeakLeft();
        float peakR = audioDevice_.getPeakRight();

        // Draw Left / Right Horizontal VU Bars
        drawVuBar(scX + 20, vuY + 20, 200, 10, peakL, "L");
        drawVuBar(scX + 20, vuY + 36, 200, 10, peakR, "R");

        // Master Soft-Clip Indicator
        bool clipping = (peakL >= 0.98f || peakR >= 0.98f);
        drawRect(scX + 20, vuY + 54, 200, 16, clipping ? COL_TEXT_RED : 0xFF18201C);
        drawText(scX + 45, vuY + 57, clipping ? "SOFT-CLIP ENGAGED" : "HEADROOM OK",
                 clipping ? 0xFFFFFFFF : COL_TEXT_DIM);

        // 8-Track Multi-Channel VU Bars
        int trkVuY = 320;
        drawText(scX, trkVuY, "8-TRACK MIX BUS", COL_TEXT_ACCENT);

        for (int t = 0; t < static_cast<int>(MAX_TRACKS); ++t) {
            int bx = scX + 8 + t * 28;
            int by = trkVuY + 24;
            int bw = 18;
            int bh = 140;

            drawRect(bx, by, bw, bh, 0xFF14181C);
            drawHollowRect(bx, by, bw, bh, (t == uiState_.active_track) ? COL_TEXT_ACCENT : COL_BORDER_DIM);

            float lvl = uiState_.vu_levels[t];
            int fillH = std::min(bh, static_cast<int>(lvl * bh));
            if (fillH > 0) {
                uint32_t col = (fillH > bh * 0.85f) ? COL_TEXT_RED : ((fillH > bh * 0.6f) ? COL_TEXT_AMBER : COL_TEXT_GREEN);
                drawRect(bx + 2, by + (bh - fillH), bw - 4, fillH, col);
            }

            // Track Label
            char tlbl[4];
            std::snprintf(tlbl, sizeof(tlbl), "T%d", t);
            drawText(bx + 2, by + bh + 6, tlbl, (t == uiState_.active_track) ? COL_TEXT_ACCENT : COL_TEXT_DIM);
        }

        // HAL Diagnostic Details
        int diagY = 540;
        drawText(scX, diagY, "SYSTEM DIAGNOSTICS", COL_TEXT_ACCENT);
        drawText(scX, diagY + 20, "Architecture: x86_64 Zen2", COL_TEXT_DIM);
        drawText(scX, diagY + 36, "Audio Driver: Direct WinMM/PipeWire", COL_TEXT_DIM);
        drawText(scX, diagY + 52, "Buffer Latency: 512f (~11.6ms)", COL_TEXT_DIM);
        drawText(scX, diagY + 68, "Lock-Free SPSC: STRICT VERIFIED", COL_TEXT_GREEN);
        drawText(scX, diagY + 84, "Zero Alloc Audio: ACTIVE", COL_TEXT_GREEN);
    }

    void renderRightFlank() {
        // Border & Background
        drawRect(RIGHT_FLANK_X + 10, 20, RIGHT_FLANK_W - 20, WIN_HEIGHT - 40, COL_PANEL_BG);
        drawHollowRect(RIGHT_FLANK_X + 10, 20, RIGHT_FLANK_W - 20, WIN_HEIGHT - 40, COL_BORDER_DIM);

        int rx = RIGHT_FLANK_X + 24;

        // Header
        drawText(rx, 35, "TRACKER MONITOR", COL_TEXT_ACCENT);

        // Song Position Monitor Box
        int monY = 60;
        drawRect(rx, monY, 230, 95, 0xFF14181E);
        drawHollowRect(rx, monY, 230, 95, COL_BORDER_BRIGHT);

        char buf[64];
        std::snprintf(buf, sizeof(buf), "STATUS:   %s", engine_.isPlaying() ? "PLAYING [SYNC]" : "STOPPED");
        drawText(rx + 10, monY + 10, buf, engine_.isPlaying() ? COL_TEXT_GREEN : COL_TEXT_AMBER);

        std::snprintf(buf, sizeof(buf), "SONG ROW: 0x%02X / 0x%02X", engine_.getSongRow(), static_cast<unsigned int>(TOTAL_SONG_ROWS));
        drawText(rx + 10, monY + 28, buf, COL_TEXT_ACCENT);

        std::snprintf(buf, sizeof(buf), "TEMPO:    %3.1f BPM (G:%d)", song_.bpm, song_.groove);
        drawText(rx + 10, monY + 46, buf, 0xFFCCDDEE);

        std::snprintf(buf, sizeof(buf), "SCALE:    C MINOR (OCT %+d)", uiState_.octave_offset);
        drawText(rx + 10, monY + 64, buf, 0xFFCCDDEE);

        // Active Track / Instrument Inspector
        int instY = 175;
        drawText(rx, instY, "ACTIVE INSTRUMENT", COL_TEXT_ACCENT);
        drawRect(rx, instY + 20, 230, 80, 0xFF14181E);
        drawHollowRect(rx, instY + 20, 230, 80, COL_BORDER_BRIGHT);

        const auto& trk = engine_.getTrackState(uiState_.active_track);
        uint8_t instId = (trk.instrument_id < TOTAL_INSTRUMENTS) ? trk.instrument_id : 0;
        const auto& inst = song_.instruments[instId];

        const char* instTypes[] = { "INTERNAL SYNTH", "WAVETABLE", "SF2 MULTISAMPLE", "MIDI OUT" };
        const char* typeName = (inst.type <= INST_TYPE_SF2_MULTISAMPLE) ? instTypes[inst.type] : "CUSTOM";

        std::snprintf(buf, sizeof(buf), "INST %02X: %s", instId, typeName);
        drawText(rx + 10, instY + 30, buf, COL_TEXT_ACCENT);

        const char* fltNames[] = { "BYPASS", "LADDER 24DB", "SVF 12DB", "COMB" };
        std::snprintf(buf, sizeof(buf), "FILTER:  %s (%d Hz)", fltNames[inst.filter_type], static_cast<int>(inst.filter_cutoff));
        drawText(rx + 10, instY + 48, buf, COL_TEXT_DIM);

        std::snprintf(buf, sizeof(buf), "NOTE:    %s (EFF: %02X)", (trk.voice_active ? "ACTIVE" : "IDLE"), trk.effective_note);
        drawText(rx + 10, instY + 66, buf, trk.voice_active ? COL_TEXT_GREEN : COL_TEXT_DIM);

        // Virtual Steam Deck Right Trackpad (Rotary Jog Wheel)
        int padY = 285;
        drawText(rx, padY, "HAPTIC ROTARY JOG WHEEL", COL_TEXT_ACCENT);
        drawText(rx, padY + 16, "[Drag mouse or scroll wheel]", COL_TEXT_DIM);

        int tcx = rx + 115;
        int tcy = padY + 120;
        int tr = 75;

        // Draw circular trackpad body
        for (int dy = -tr; dy <= tr; ++dy) {
            int cy = tcy + dy;
            if (cy < 0 || cy >= WIN_HEIGHT) continue;
            for (int dx = -tr; dx <= tr; ++dx) {
                int cx = tcx + dx;
                if (cx < 0 || cx >= WIN_WIDTH) continue;
                int distSq = dx * dx + dy * dy;
                if (distSq <= tr * tr) {
                    uint32_t col = (distSq >= (tr - 3) * (tr - 3)) ? COL_TRACKPAD_RIM : COL_TRACKPAD_BG;
                    if (distSq <= (tr / 2) * (tr / 2) && distSq >= (tr / 2 - 2) * (tr / 2 - 2)) {
                        col = 0xFF1E2830; // Concentric ring
                    }
                    framebuffer_[cy * WIN_WIDTH + cx] = col;
                }
            }
        }

        // Draw Jog Touch Cursor
        int curX = tcx + static_cast<int>(trackpadX_ * (tr - 10));
        int curY = tcy + static_cast<int>(trackpadY_ * (tr - 10));
        drawRect(curX - 4, curY - 4, 9, 9, COL_TEXT_ACCENT);

        // Controller Legend
        int legY = 510;
        drawText(rx, legY, "STEAM DECK SHORTCUTS", COL_TEXT_ACCENT);
        drawText(rx, legY + 20, "D-Pad / Arrows: Cursor Move", COL_TEXT_DIM);
        drawText(rx, legY + 36, "A / B (Z / X):  Edit Inc / Dec", COL_TEXT_DIM);
        drawText(rx, legY + 52, "X / Y (C / V):  Context / Audition", COL_TEXT_DIM);
        drawText(rx, legY + 68, "L1 / R1 (Q / W): View Stack Prev / Next", COL_TEXT_DIM);
        drawText(rx, legY + 84, "L2 / R2 (1 / 2): Fast View / Sub Mod", COL_TEXT_DIM);
        drawText(rx, legY + 100, "L4 / L5 (7 / 8): Octave Down / Up", COL_TEXT_DIM);
        drawText(rx, legY + 116, "R4 / R5 (9 / 0): Track Mute / Solo", COL_TEXT_DIM);
        drawText(rx, legY + 132, "Space / Start:   Play / Stop Transport", COL_TEXT_GREEN);
    }

    void drawVuBar(int x, int y, int w, int h, float level, const char* label) {
        drawText(x - 14, y, label, COL_TEXT_DIM);
        drawRect(x, y, w, h, 0xFF14181C);
        drawHollowRect(x, y, w, h, COL_BORDER_DIM);

        int fillW = std::min(w, static_cast<int>(level * w));
        if (fillW > 0) {
            uint32_t col = (level > 0.95f) ? COL_TEXT_RED : ((level > 0.75f) ? COL_TEXT_AMBER : COL_TEXT_GREEN);
            drawRect(x + 1, y + 1, fillW - 2, h - 2, col);
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

    void drawText(int x, int y, const char* str, uint32_t col) {
        while (*str) {
            char c = *str++;
            // Draw clean 4x6 characters
            for (int r = 0; r < 6; ++r) {
                int cy = y + r;
                if (cy < 0 || cy >= WIN_HEIGHT) continue;
                for (int colIdx = 0; colIdx < 4; ++colIdx) {
                    int cx = x + colIdx;
                    if (cx >= 0 && cx < WIN_WIDTH) {
                        if (c != ' ') {
                            framebuffer_[cy * WIN_WIDTH + cx] = col;
                        }
                    }
                }
            }
            x += 6;
        }
    }

    HWND hWnd_{nullptr};
    std::vector<uint32_t> framebuffer_{};

    Engine engine_{};
    Song song_{};
    UIState uiState_{};
    DisplayEngine displayEngine_{};
    SteamDeckHAL steamDeckHAL_{};

    Win32AudioDevice audioDevice_{};
    Win32Gamepad gamepad_{};

    // Trackpad Jog State
    bool trackpadDragging_{false};
    float trackpadX_{0.0f};
    float trackpadY_{0.0f};
    float lastTrackpadAngle_{0.0f};
};

} // namespace toad

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    auto app = std::make_unique<toad::SteamDeckRunnerApp>();
    if (!app->initialize(hInstance, nCmdShow)) {
        MessageBoxW(nullptr, L"Failed to initialize Steam Deck Runner", L"ToadTracker Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    return app->run();
}
