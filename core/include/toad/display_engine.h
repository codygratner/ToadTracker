#pragma once

#include "toad/types.h"
#include "toad/engine.h"
#include "toad/ui_state.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace toad {

// ============================================================================
// VIRTUAL TBD-16 DISPLAY ENGINE (240x240 Native Canvas)
// Zero dynamic allocation software rasterizer
// ============================================================================
class DisplayEngine {
public:
    static constexpr size_t SCREEN_WIDTH = 240;
    static constexpr size_t SCREEN_HEIGHT = 240;
    static constexpr size_t TOTAL_PIXELS = SCREEN_WIDTH * SCREEN_HEIGHT;

    // Steam Deck Target Dimensions
    static constexpr size_t STEAM_DECK_WIDTH = 1280;
    static constexpr size_t STEAM_DECK_HEIGHT = 800;
    static constexpr size_t STEAM_DECK_SCALE = 3; // 3x integer scaling: 720x720
    static constexpr size_t STEAM_DECK_OFFSET_X = (STEAM_DECK_WIDTH - SCREEN_WIDTH * STEAM_DECK_SCALE) / 2; // 280 px
    static constexpr size_t STEAM_DECK_OFFSET_Y = (STEAM_DECK_HEIGHT - SCREEN_HEIGHT * STEAM_DECK_SCALE) / 2; // 40 px

    // Palette Colors (32-bit ARGB & 16-bit RGB565)
    struct Colors {
        static constexpr uint32_t BG_OBSIDIAN       = 0xFF081008; // Deep Tsathoggua moss
        static constexpr uint32_t TEXT_BRIGHT       = 0xFF33FF66; // Phosphor emerald green
        static constexpr uint32_t TEXT_DIM          = 0xFF185528; // Dim background text
        static constexpr uint32_t TEXT_ACCENT       = 0xFF55FFFF; // Cyber cyan
        static constexpr uint32_t TEXT_WARN         = 0xFFFFCC00; // Amber warning
        static constexpr uint32_t TEXT_ALERT        = 0xFFFF3333; // Red alert / Master clip
        static constexpr uint32_t CURSOR_BG         = 0xFF00FF88; // Cursor highlight block
        static constexpr uint32_t CURSOR_TEXT       = 0xFF000000; // Text on cursor
        static constexpr uint32_t PLAYHEAD_BAR      = 0xFF143018; // Playhead row bar
        static constexpr uint32_t GRID_LINE         = 0xFF102014; // Subtle separator
    };

    DisplayEngine() {
        clear(Colors::BG_OBSIDIAN);
    }

    // --- COLOR CONVERSION ---
    static inline uint16_t argbToRgb565(uint32_t c) {
        uint8_t r = (c >> 16) & 0xFF;
        uint8_t g = (c >> 8) & 0xFF;
        uint8_t b = c & 0xFF;
        return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }

    // --- LOW-LEVEL PRIMITIVES ---
    void clear(uint32_t color = Colors::BG_OBSIDIAN) {
        std::fill(bufferArgb_, bufferArgb_ + TOTAL_PIXELS, color);
        uint16_t rgb565 = argbToRgb565(color);
        std::fill(bufferRgb565_, bufferRgb565_ + TOTAL_PIXELS, rgb565);
    }

    inline void drawPixel(int x, int y, uint32_t color) {
        if (x < 0 || x >= static_cast<int>(SCREEN_WIDTH) || y < 0 || y >= static_cast<int>(SCREEN_HEIGHT)) return;
        size_t idx = static_cast<size_t>(y * SCREEN_WIDTH + x);
        bufferArgb_[idx] = color;
        bufferRgb565_[idx] = argbToRgb565(color);
    }

    void fillRect(int x, int y, int w, int h, uint32_t color) {
        int x0 = std::max(0, x);
        int y0 = std::max(0, y);
        int x1 = std::min(static_cast<int>(SCREEN_WIDTH), x + w);
        int y1 = std::min(static_cast<int>(SCREEN_HEIGHT), y + h);

        uint16_t rgb565 = argbToRgb565(color);
        for (int cy = y0; cy < y1; ++cy) {
            size_t rowOffset = static_cast<size_t>(cy * SCREEN_WIDTH);
            for (int cx = x0; cx < x1; ++cx) {
                bufferArgb_[rowOffset + cx] = color;
                bufferRgb565_[rowOffset + cx] = rgb565;
            }
        }
    }

    void drawHLine(int x, int y, int w, uint32_t color) {
        fillRect(x, y, w, 1, color);
    }

    void drawVLine(int x, int y, int h, uint32_t color) {
        fillRect(x, y, 1, h, color);
    }

    // --- 4x6 MONOSPACE BITMAP FONT ---
    // Zero-allocation glyph rendering: digits 0-9, A-Z, punctuation, notes
    void drawChar(int x, int y, char c, uint32_t color, uint32_t bgColor = 0) {
        // Simple 4x6 font table (bitmapped 4 cols x 6 rows)
        static const uint8_t font4x6[][6] = {
            // Space
            {0x0, 0x0, 0x0, 0x0, 0x0, 0x0}, // ' ' (32)
            // '!'
            {0x4, 0x4, 0x4, 0x0, 0x4, 0x0}, // '!'
            // '"'
            {0xA, 0xA, 0x0, 0x0, 0x0, 0x0},
            // '#'
            {0xA, 0xF, 0xA, 0xF, 0xA, 0x0},
            // '$'
            {0x4, 0xF, 0x6, 0x9, 0xF, 0x4},
            // '%'
            {0x9, 0x2, 0x4, 0x8, 0x9, 0x0},
            // '&'
            {0x6, 0x9, 0x6, 0x9, 0x7, 0x0},
            // '\''
            {0x4, 0x4, 0x0, 0x0, 0x0, 0x0},
            // '('
            {0x2, 0x4, 0x4, 0x4, 0x2, 0x0},
            // ')'
            {0x8, 0x4, 0x4, 0x4, 0x8, 0x0},
            // '*'
            {0x0, 0xA, 0x4, 0xA, 0x0, 0x0},
            // '+'
            {0x0, 0x4, 0xE, 0x4, 0x0, 0x0},
            // ','
            {0x0, 0x0, 0x0, 0x4, 0x4, 0x8},
            // '-'
            {0x0, 0x0, 0xE, 0x0, 0x0, 0x0},
            // '.'
            {0x0, 0x0, 0x0, 0x0, 0x4, 0x0},
            // '/'
            {0x1, 0x2, 0x4, 0x8, 0x0, 0x0},
            // Digits '0' - '9' (index 16..25)
            {0xE, 0xA, 0xA, 0xA, 0xE, 0x0}, // '0'
            {0x4, 0xC, 0x4, 0x4, 0xE, 0x0}, // '1'
            {0xE, 0x2, 0xE, 0x8, 0xE, 0x0}, // '2'
            {0xE, 0x2, 0x6, 0x2, 0xE, 0x0}, // '3'
            {0xA, 0xA, 0xE, 0x2, 0x2, 0x0}, // '4'
            {0xE, 0x8, 0xE, 0x2, 0xE, 0x0}, // '5'
            {0xE, 0x8, 0xE, 0xA, 0xE, 0x0}, // '6'
            {0xE, 0x2, 0x4, 0x4, 0x4, 0x0}, // '7'
            {0xE, 0xA, 0xE, 0xA, 0xE, 0x0}, // '8'
            {0xE, 0xA, 0xE, 0x2, 0xE, 0x0}, // '9'
            // ':'
            {0x0, 0x4, 0x0, 0x4, 0x0, 0x0},
            // ';'
            {0x0, 0x4, 0x0, 0x4, 0x4, 0x8},
            // '<'
            {0x2, 0x4, 0x8, 0x4, 0x2, 0x0},
            // '='
            {0x0, 0xE, 0x0, 0xE, 0x0, 0x0},
            // '>'
            {0x8, 0x4, 0x2, 0x4, 0x8, 0x0},
            // '?'
            {0xE, 0x2, 0x6, 0x0, 0x4, 0x0},
            // '@'
            {0xE, 0xB, 0xD, 0x9, 0xE, 0x0},
            // Uppercase 'A' - 'Z' (index 33..58)
            {0x6, 0x9, 0xF, 0x9, 0x9, 0x0}, // 'A'
            {0xE, 0x9, 0xE, 0x9, 0xE, 0x0}, // 'B'
            {0x7, 0x8, 0x8, 0x8, 0x7, 0x0}, // 'C'
            {0xE, 0x9, 0x9, 0x9, 0xE, 0x0}, // 'D'
            {0xF, 0x8, 0xE, 0x8, 0xF, 0x0}, // 'E'
            {0xF, 0x8, 0xE, 0x8, 0x8, 0x0}, // 'F'
            {0x7, 0x8, 0xB, 0x9, 0x7, 0x0}, // 'G'
            {0x9, 0x9, 0xF, 0x9, 0x9, 0x0}, // 'H'
            {0xE, 0x4, 0x4, 0x4, 0xE, 0x0}, // 'I'
            {0x2, 0x2, 0x2, 0xA, 0x4, 0x0}, // 'J'
            {0x9, 0xA, 0xC, 0xA, 0x9, 0x0}, // 'K'
            {0x8, 0x8, 0x8, 0x8, 0xF, 0x0}, // 'L'
            {0x9, 0xF, 0x9, 0x9, 0x9, 0x0}, // 'M'
            {0x9, 0xD, 0xB, 0x9, 0x9, 0x0}, // 'N'
            {0x6, 0x9, 0x9, 0x9, 0x6, 0x0}, // 'O'
            {0xE, 0x9, 0xE, 0x8, 0x8, 0x0}, // 'P'
            {0x6, 0x9, 0x9, 0xD, 0x7, 0x0}, // 'Q'
            {0xE, 0x9, 0xE, 0xA, 0x9, 0x0}, // 'R'
            {0x7, 0x8, 0x6, 0x1, 0xE, 0x0}, // 'S'
            {0xF, 0x4, 0x4, 0x4, 0x4, 0x0}, // 'T'
            {0x9, 0x9, 0x9, 0x9, 0x6, 0x0}, // 'U'
            {0x9, 0x9, 0x9, 0x5, 0x2, 0x0}, // 'V'
            {0x9, 0x9, 0x9, 0xF, 0x9, 0x0}, // 'W'
            {0x9, 0x9, 0x6, 0x9, 0x9, 0x0}, // 'X'
            {0x9, 0x9, 0x6, 0x2, 0x2, 0x0}, // 'Y'
            {0xF, 0x2, 0x4, 0x8, 0xF, 0x0}  // 'Z'
        };

        if (bgColor != 0) {
            fillRect(x, y, 5, 7, bgColor);
        }

        uint8_t glyphIdx = 0;
        char uc = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
        if (uc >= 32 && uc <= 'Z') {
            glyphIdx = static_cast<uint8_t>(uc - 32);
        }

        const uint8_t* rows = font4x6[glyphIdx];
        for (int r = 0; r < 6; ++r) {
            uint8_t rowVal = rows[r];
            for (int col = 0; col < 4; ++col) {
                if ((rowVal >> (3 - col)) & 1) {
                    drawPixel(x + col, y + r, color);
                }
            }
        }
    }

    void drawString(int x, int y, const char* str, uint32_t color, uint32_t bgColor = 0) {
        int cx = x;
        while (*str) {
            drawChar(cx, y, *str++, color, bgColor);
            cx += 5; // 4 px width + 1 px spacing
        }
    }

    void drawHexByte(int x, int y, uint8_t val, uint32_t color, uint32_t bgColor = 0) {
        static const char hexChars[] = "0123456789ABCDEF";
        drawChar(x, y, hexChars[(val >> 4) & 0x0F], color, bgColor);
        drawChar(x + 5, y, hexChars[val & 0x0F], color, bgColor);
    }

    void drawNote(int x, int y, uint8_t note, uint32_t color, uint32_t bgColor = 0) {
        if (note == NOTE_EMPTY) {
            drawString(x, y, "---", Colors::TEXT_DIM, bgColor);
            return;
        }

        static const char* noteNames[] = {
            "C-", "C#", "D-", "D#", "E-", "F-",
            "F#", "G-", "G#", "A-", "A#", "B-"
        };

        int octave = note / 12;
        int semitone = note % 12;
        char str[4];
        str[0] = noteNames[semitone][0];
        str[1] = noteNames[semitone][1];
        str[2] = static_cast<char>('0' + std::min(9, std::max(0, octave)));
        str[3] = '\0';

        drawString(x, y, str, color, bgColor);
    }

    // --- HIGH-LEVEL VIEW RENDERERS ---
    void renderHeader(const UIState& ui, const Song& song, const Engine& engine) {
        // Top 14 px header
        fillRect(0, 0, SCREEN_WIDTH, 14, Colors::BG_OBSIDIAN);
        drawHLine(0, 13, SCREEN_WIDTH, Colors::GRID_LINE);

        // View Name
        static const char* viewNames[] = {
            "SONG", "CHAIN", "PHRASE", "TABLE", "INST", "SYNTH", "PROJ"
        };
        const char* vName = (ui.current_view < VIEW_COUNT) ? viewNames[ui.current_view] : "VIEW";
        drawString(4, 3, vName, Colors::TEXT_ACCENT);

        // Song Row / Transport
        char status[32];
        std::snprintf(status, sizeof(status), "%s R%02X %03dBPM",
                      engine.isPlaying() ? ">" : "[]",
                      static_cast<unsigned int>(engine.getSongRow()),
                      static_cast<int>(song.bpm));
        drawString(110, 3, status, engine.isPlaying() ? Colors::TEXT_BRIGHT : Colors::TEXT_WARN);
    }

    void renderFooter(const UIState& ui) {
        // Bottom 16 px footer
        fillRect(0, SCREEN_HEIGHT - 16, SCREEN_WIDTH, 16, Colors::BG_OBSIDIAN);
        drawHLine(0, SCREEN_HEIGHT - 16, SCREEN_WIDTH, Colors::GRID_LINE);

        // Render 8 Track Mini-VU Bars
        for (int t = 0; t < static_cast<int>(MAX_TRACKS); ++t) {
            int bx = 4 + t * 14;
            int by = SCREEN_HEIGHT - 12;
            int bw = 10;
            int bh = 8;

            fillRect(bx, by, bw, bh, Colors::GRID_LINE);

            int vuHeight = std::min(bh, static_cast<int>(ui.vu_levels[t] * bh));
            if (vuHeight > 0) {
                uint32_t col = (t == ui.active_track) ? Colors::TEXT_ACCENT : Colors::TEXT_BRIGHT;
                fillRect(bx, by + (bh - vuHeight), bw, vuHeight, col);
            }

            if (ui.track_muted[t]) {
                drawChar(bx + 3, by + 1, 'M', Colors::TEXT_ALERT);
            }
        }

        // Active track & Octave info
        char info[24];
        std::snprintf(info, sizeof(info), "T%d OCT%+d", ui.active_track, ui.octave_offset);
        drawString(170, SCREEN_HEIGHT - 11, info, Colors::TEXT_ACCENT);
    }

    void renderSongView(const UIState& ui, const Song& song, const Engine& engine) {
        int startY = 16;
        int rowHeight = 11;
        int visibleRows = 16;

        int scrollRow = std::max(0, ui.cursor_row - 7);
        if (scrollRow + visibleRows > static_cast<int>(TOTAL_SONG_ROWS)) {
            scrollRow = static_cast<int>(TOTAL_SONG_ROWS) - visibleRows;
        }

        for (int r = 0; r < visibleRows; ++r) {
            int rowIdx = scrollRow + r;
            int y = startY + r * rowHeight;

            bool isPlayhead = (rowIdx == static_cast<int>(engine.getSongRow()) && engine.isPlaying());
            if (isPlayhead) {
                fillRect(0, y, SCREEN_WIDTH, rowHeight - 1, Colors::PLAYHEAD_BAR);
            }

            // Row Index
            drawHexByte(4, y + 2, static_cast<uint8_t>(rowIdx), isPlayhead ? Colors::TEXT_ACCENT : Colors::TEXT_DIM);

            // 8 Tracks
            for (int t = 0; t < static_cast<int>(MAX_TRACKS); ++t) {
                int x = 32 + t * 25;
                uint8_t chainId = song.rows[rowIdx].chain_ids[t];

                bool isCursor = (rowIdx == ui.cursor_row && t == ui.cursor_col);
                uint32_t bg = isCursor ? Colors::CURSOR_BG : 0;
                uint32_t fg = isCursor ? Colors::CURSOR_TEXT : ((chainId == CHAIN_EMPTY) ? Colors::TEXT_DIM : Colors::TEXT_BRIGHT);

                if (chainId == CHAIN_EMPTY) {
                    drawString(x, y + 2, "--", fg, bg);
                } else {
                    drawHexByte(x, y + 2, chainId, fg, bg);
                }
            }
        }
    }

    void renderChainView(const UIState& ui, const Song& song) {
        int startY = 16;
        int rowHeight = 11;
        const Chain& ch = song.chains[ui.selected_chain_id < TOTAL_CHAINS ? ui.selected_chain_id : 0];

        // Header info
        char title[32];
        std::snprintf(title, sizeof(title), "CHAIN %02X", ui.selected_chain_id);
        drawString(4, startY + 2, title, Colors::TEXT_ACCENT);

        startY += 14;
        for (int s = 0; s < 16; ++s) {
            int y = startY + s * rowHeight;

            // Step number
            drawHexByte(8, y + 2, static_cast<uint8_t>(s), Colors::TEXT_DIM);

            // Phrase ID
            bool isCursorPhrase = (s == ui.cursor_row && ui.cursor_col == 0);
            uint32_t bgP = isCursorPhrase ? Colors::CURSOR_BG : 0;
            uint32_t fgP = isCursorPhrase ? Colors::CURSOR_TEXT : ((ch.steps[s].phrase_id == PHRASE_EMPTY) ? Colors::TEXT_DIM : Colors::TEXT_BRIGHT);

            if (ch.steps[s].phrase_id == PHRASE_EMPTY) {
                drawString(40, y + 2, "--", fgP, bgP);
            } else {
                drawHexByte(40, y + 2, ch.steps[s].phrase_id, fgP, bgP);
            }

            // Transposition
            bool isCursorTrans = (s == ui.cursor_row && ui.cursor_col == 1);
            uint32_t bgT = isCursorTrans ? Colors::CURSOR_BG : 0;
            uint32_t fgT = isCursorTrans ? Colors::CURSOR_TEXT : Colors::TEXT_WARN;

            char transStr[8];
            std::snprintf(transStr, sizeof(transStr), "%+03d", ch.steps[s].transpose);
            drawString(80, y + 2, transStr, fgT, bgT);
        }
    }

    void renderPhraseView(const UIState& ui, const Song& song, const Engine& engine) {
        int startY = 16;
        int rowHeight = 11;
        const Phrase& ph = song.phrases[ui.selected_phrase_id < TOTAL_PHRASES ? ui.selected_phrase_id : 0];

        // Column Titles
        drawString(4, startY, "ST", Colors::TEXT_DIM);
        drawString(24, startY, "NOTE", Colors::TEXT_DIM);
        drawString(54, startY, "IN", Colors::TEXT_DIM);
        drawString(74, startY, "VL", Colors::TEXT_DIM);
        drawString(98, startY, "FX1", Colors::TEXT_DIM);
        drawString(142, startY, "FX2", Colors::TEXT_DIM);
        drawString(186, startY, "FX3", Colors::TEXT_DIM);

        startY += 10;
        uint8_t activePhraseStep = engine.getTrackState(ui.active_track).phrase_step;

        for (int s = 0; s < 16; ++s) {
            int y = startY + s * rowHeight;
            const PhraseStep& step = ph.steps[s];

            bool isPlayhead = (s == activePhraseStep && engine.isPlaying());
            if (isPlayhead) {
                fillRect(0, y, SCREEN_WIDTH, rowHeight - 1, Colors::PLAYHEAD_BAR);
            }

            // Step
            drawHexByte(4, y + 2, static_cast<uint8_t>(s), isPlayhead ? Colors::TEXT_ACCENT : Colors::TEXT_DIM);

            // Note (Col 0)
            bool cur0 = (s == ui.cursor_row && ui.cursor_col == 0);
            drawNote(24, y + 2, step.note, cur0 ? Colors::CURSOR_TEXT : Colors::TEXT_BRIGHT, cur0 ? Colors::CURSOR_BG : 0);

            // Inst (Col 1)
            bool cur1 = (s == ui.cursor_row && ui.cursor_col == 1);
            drawHexByte(54, y + 2, step.instrument, cur1 ? Colors::CURSOR_TEXT : Colors::TEXT_WARN, cur1 ? Colors::CURSOR_BG : 0);

            // Vol (Col 2)
            bool cur2 = (s == ui.cursor_row && ui.cursor_col == 2);
            drawHexByte(74, y + 2, step.volume, cur2 ? Colors::CURSOR_TEXT : Colors::TEXT_BRIGHT, cur2 ? Colors::CURSOR_BG : 0);

            // FX Columns (Col 3..8)
            static const char* cmdNames[] = {
                "--", "VOL", "CUT", "RES", "FLD", "PWM", "DSP", "WPS", "WAM", "DRV", "ATB", "HOP"
            };

            for (int f = 0; f < 3; ++f) {
                int fxX = 98 + f * 44;
                bool curCmd = (s == ui.cursor_row && ui.cursor_col == (3 + f * 2));
                bool curVal = (s == ui.cursor_row && ui.cursor_col == (4 + f * 2));

                const char* cName = (step.fx[f].cmd <= CMD_HOP_) ? cmdNames[step.fx[f].cmd] : "??";
                drawString(fxX, y + 2, cName, curCmd ? Colors::CURSOR_TEXT : Colors::TEXT_ACCENT, curCmd ? Colors::CURSOR_BG : 0);
                drawHexByte(fxX + 18, y + 2, step.fx[f].val, curVal ? Colors::CURSOR_TEXT : Colors::TEXT_BRIGHT, curVal ? Colors::CURSOR_BG : 0);
            }
        }
    }

    void renderTableView(const UIState& ui, const Song& song) {
        int startY = 20;
        int rowHeight = 11;
        const Table& tb = song.tables[ui.selected_table_id < TOTAL_TABLES ? ui.selected_table_id : 0];

        char title[32];
        std::snprintf(title, sizeof(title), "TABLE %02X  SPD %d  %s", ui.selected_table_id, tb.speed, tb.loop ? "LOOP" : "ONCE");
        drawString(4, startY, title, Colors::TEXT_ACCENT);

        startY += 14;
        drawString(4, startY, "RW", Colors::TEXT_DIM);
        drawString(28, startY, "TRN", Colors::TEXT_DIM);
        drawString(60, startY, "VOL", Colors::TEXT_DIM);
        drawString(88, startY, "FX1", Colors::TEXT_DIM);
        drawString(140, startY, "FX2", Colors::TEXT_DIM);

        startY += 10;
        for (int r = 0; r < 16; ++r) {
            int y = startY + r * rowHeight;
            const TableRow& row = tb.rows[r];

            drawHexByte(4, y + 2, static_cast<uint8_t>(r), Colors::TEXT_DIM);

            // Transpose (Col 0)
            bool cur0 = (r == ui.cursor_row && ui.cursor_col == 0);
            char trStr[8];
            std::snprintf(trStr, sizeof(trStr), "%+03d", row.transpose);
            drawString(28, y + 2, trStr, cur0 ? Colors::CURSOR_TEXT : Colors::TEXT_WARN, cur0 ? Colors::CURSOR_BG : 0);

            // Volume (Col 1)
            bool cur1 = (r == ui.cursor_row && ui.cursor_col == 1);
            drawHexByte(60, y + 2, row.volume, cur1 ? Colors::CURSOR_TEXT : Colors::TEXT_BRIGHT, cur1 ? Colors::CURSOR_BG : 0);

            // FX1 & FX2
            static const char* cmdNames[] = {
                "--", "VOL", "CUT", "RES", "FLD", "PWM", "DSP", "WPS", "WAM", "DRV", "ATB", "HOP"
            };
            const char* c1 = (row.cmd1 <= CMD_HOP_) ? cmdNames[row.cmd1] : "??";
            const char* c2 = (row.cmd2 <= CMD_HOP_) ? cmdNames[row.cmd2] : "??";

            bool curC1 = (r == ui.cursor_row && ui.cursor_col == 2);
            bool curV1 = (r == ui.cursor_row && ui.cursor_col == 3);
            drawString(88, y + 2, c1, curC1 ? Colors::CURSOR_TEXT : Colors::TEXT_ACCENT, curC1 ? Colors::CURSOR_BG : 0);
            drawHexByte(106, y + 2, row.val1, curV1 ? Colors::CURSOR_TEXT : Colors::TEXT_BRIGHT, curV1 ? Colors::CURSOR_BG : 0);

            bool curC2 = (r == ui.cursor_row && ui.cursor_col == 4);
            bool curV2 = (r == ui.cursor_row && ui.cursor_col == 5);
            drawString(140, y + 2, c2, curC2 ? Colors::CURSOR_TEXT : Colors::TEXT_ACCENT, curC2 ? Colors::CURSOR_BG : 0);
            drawHexByte(158, y + 2, row.val2, curV2 ? Colors::CURSOR_TEXT : Colors::TEXT_BRIGHT, curV2 ? Colors::CURSOR_BG : 0);
        }
    }

    void renderSynthView(const UIState& ui, const Song& song) {
        int startY = 24;
        const Instrument& inst = song.instruments[ui.selected_instrument_id < TOTAL_INSTRUMENTS ? ui.selected_instrument_id : 0];

        char title[48];
        std::snprintf(title, sizeof(title), "INST %02X: %s",
                      ui.selected_instrument_id,
                      (inst.type == INST_TYPE_INTERNAL_SYNTH) ? "ALPHA JUNO SYNTH" :
                      (inst.type == INST_TYPE_WAVETABLE) ? "WAVETABLE SYNTH" : "SF2 PLAYER");
        drawString(4, startY, title, Colors::TEXT_ACCENT);

        startY += 20;

        if (inst.type == INST_TYPE_INTERNAL_SYNTH) {
            static const char* synthParamLabels[] = {
                "PULSE WIDTH", "WAVEFOLDER", "DISP FREQ", "DISP STAGES"
            };

            for (int p = 0; p < 4; ++p) {
                int y = startY + p * 20;
                bool isCursor = (p == ui.cursor_row);
                uint32_t fg = isCursor ? Colors::CURSOR_TEXT : Colors::TEXT_BRIGHT;
                uint32_t bg = isCursor ? Colors::CURSOR_BG : 0;

                drawString(8, y, synthParamLabels[p], fg, bg);

                // Slider Bar
                int barX = 110;
                int barW = 100;
                int barH = 7;
                fillRect(barX, y, barW, barH, Colors::GRID_LINE);

                float valNorm = 0.0f;
                if (p == 0) valNorm = inst.synth.pulse_width;
                else if (p == 1) valNorm = inst.synth.fold_drive / 4.0f;
                else if (p == 2) valNorm = (inst.synth.disperser_freq - 20.0f) / 19980.0f;
                else if (p == 3) valNorm = static_cast<float>(inst.synth.disperser_stages) / 8.0f;

                int fillW = std::min(barW, std::max(0, static_cast<int>(valNorm * barW)));
                fillRect(barX, y, fillW, barH, Colors::TEXT_ACCENT);
            }
        } else if (inst.type == INST_TYPE_WAVETABLE) {
            static const char* wtLabels[] = {
                "WARP MODE", "WARP AMOUNT", "WT POSITION"
            };

            for (int p = 0; p < 3; ++p) {
                int y = startY + p * 20;
                bool isCursor = (p == ui.cursor_row);
                uint32_t fg = isCursor ? Colors::CURSOR_TEXT : Colors::TEXT_BRIGHT;
                uint32_t bg = isCursor ? Colors::CURSOR_BG : 0;

                drawString(8, y, wtLabels[p], fg, bg);

                int barX = 110;
                int barW = 100;
                int barH = 7;
                fillRect(barX, y, barW, barH, Colors::GRID_LINE);

                float valNorm = 0.0f;
                if (p == 0) valNorm = static_cast<float>(inst.wavetable.warp_mode) / static_cast<float>(WARP_BITCR);
                else if (p == 1) valNorm = inst.wavetable.warp_amount;
                else if (p == 2) valNorm = inst.wavetable.position / 63.0f;

                int fillW = std::min(barW, std::max(0, static_cast<int>(valNorm * barW)));
                fillRect(barX, y, fillW, barH, Colors::TEXT_ACCENT);
            }
        }
    }

    // --- MAIN RENDER ENTRY POINT ---
    void render(const UIState& ui, const Song& song, const Engine& engine) {
        clear(Colors::BG_OBSIDIAN);
        renderHeader(ui, song, engine);

        switch (ui.current_view) {
            case VIEW_SONG:
                renderSongView(ui, song, engine);
                break;
            case VIEW_CHAIN:
                renderChainView(ui, song);
                break;
            case VIEW_PHRASE:
                renderPhraseView(ui, song, engine);
                break;
            case VIEW_TABLE:
                renderTableView(ui, song);
                break;
            case VIEW_SYNTH:
            case VIEW_INSTRUMENT:
                renderSynthView(ui, song);
                break;
            default:
                renderSongView(ui, song, engine);
                break;
        }

        renderFooter(ui);
    }

    // --- STEAM DECK 1280x800 COMPOSITOR ---
    // Zero dynamic allocation: blits 240x240 buffer scaled 3x (720x720) centered on target buffer
    void compositeSteamDeck(uint32_t* target1280x800, const UIState& ui, const Engine& engine) const {
        if (!target1280x800) return;

        // Fill background with dark slate
        std::fill(target1280x800, target1280x800 + (STEAM_DECK_WIDTH * STEAM_DECK_HEIGHT), 0xFF050808);

        // Blit 3x scaled center canvas (720x720 at X=280, Y=40)
        for (size_t y = 0; y < SCREEN_HEIGHT; ++y) {
            size_t srcRow = y * SCREEN_WIDTH;
            size_t dstYBase = (STEAM_DECK_OFFSET_Y + y * STEAM_DECK_SCALE);

            for (size_t scaleY = 0; scaleY < STEAM_DECK_SCALE; ++scaleY) {
                size_t dstRow = (dstYBase + scaleY) * STEAM_DECK_WIDTH + STEAM_DECK_OFFSET_X;
                for (size_t x = 0; x < SCREEN_WIDTH; ++x) {
                    uint32_t px = bufferArgb_[srcRow + x];
                    for (size_t scaleX = 0; scaleX < STEAM_DECK_SCALE; ++scaleX) {
                        target1280x800[dstRow + x * STEAM_DECK_SCALE + scaleX] = px;
                    }
                }
            }
        }

        // Draw lateral sidebar visualizers (Left Sidebar: VU Meters / Scope, Right Sidebar: Active Tracks)
        // Left flank border
        for (size_t y = STEAM_DECK_OFFSET_Y; y < STEAM_DECK_OFFSET_Y + (SCREEN_HEIGHT * STEAM_DECK_SCALE); ++y) {
            target1280x800[y * STEAM_DECK_WIDTH + (STEAM_DECK_OFFSET_X - 1)] = Colors::GRID_LINE;
            target1280x800[y * STEAM_DECK_WIDTH + (STEAM_DECK_OFFSET_X + SCREEN_WIDTH * STEAM_DECK_SCALE)] = Colors::GRID_LINE;
        }
    }

    // --- BUFFER ACCESSORS ---
    [[nodiscard]] const uint16_t* getRgb565Buffer() const noexcept { return bufferRgb565_; }
    [[nodiscard]] const uint32_t* getArgbBuffer() const noexcept { return bufferArgb_; }

private:
    alignas(64) uint32_t bufferArgb_[TOTAL_PIXELS]{};
    alignas(64) uint16_t bufferRgb565_[TOTAL_PIXELS]{};
};

} // namespace toad
