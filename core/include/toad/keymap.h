#pragma once

#include "input.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <array>
#include <string>
#include <algorithm>
#include <fstream>

namespace toad {

// Virtual Key Code Constants (Standard ASCII & Win32 VK values)
#ifndef VK_SPACE
constexpr uint32_t TOAD_VK_BACK      = 0x08;
constexpr uint32_t TOAD_VK_TAB       = 0x09;
constexpr uint32_t TOAD_VK_RETURN    = 0x0D;
constexpr uint32_t TOAD_VK_SHIFT     = 0x10;
constexpr uint32_t TOAD_VK_CONTROL   = 0x11;
constexpr uint32_t TOAD_VK_ESCAPE    = 0x1B;
constexpr uint32_t TOAD_VK_SPACE     = 0x20;
constexpr uint32_t TOAD_VK_PRIOR     = 0x21; // Page Up
constexpr uint32_t TOAD_VK_NEXT      = 0x22; // Page Down
constexpr uint32_t TOAD_VK_LEFT      = 0x25;
constexpr uint32_t TOAD_VK_UP        = 0x26;
constexpr uint32_t TOAD_VK_RIGHT     = 0x27;
constexpr uint32_t TOAD_VK_DOWN      = 0x28;
constexpr uint32_t TOAD_VK_OEM_PLUS  = 0xBB; // = / +
constexpr uint32_t TOAD_VK_OEM_COMMA = 0xBC; // , / <
constexpr uint32_t TOAD_VK_OEM_MINUS = 0xBD; // - / _
constexpr uint32_t TOAD_VK_OEM_PER   = 0xBE; // . / >
constexpr uint32_t TOAD_VK_OEM_4     = 0xDB; // [ / {
constexpr uint32_t TOAD_VK_OEM_6     = 0xDD; // ] / }
#else
constexpr uint32_t TOAD_VK_BACK      = VK_BACK;
constexpr uint32_t TOAD_VK_TAB       = VK_TAB;
constexpr uint32_t TOAD_VK_RETURN    = VK_RETURN;
constexpr uint32_t TOAD_VK_SHIFT     = VK_SHIFT;
constexpr uint32_t TOAD_VK_CONTROL   = VK_CONTROL;
constexpr uint32_t TOAD_VK_ESCAPE    = VK_ESCAPE;
constexpr uint32_t TOAD_VK_SPACE     = VK_SPACE;
constexpr uint32_t TOAD_VK_PRIOR     = VK_PRIOR;
constexpr uint32_t TOAD_VK_NEXT      = VK_NEXT;
constexpr uint32_t TOAD_VK_LEFT      = VK_LEFT;
constexpr uint32_t TOAD_VK_UP        = VK_UP;
constexpr uint32_t TOAD_VK_RIGHT     = VK_RIGHT;
constexpr uint32_t TOAD_VK_DOWN      = VK_DOWN;
constexpr uint32_t TOAD_VK_OEM_PLUS  = VK_OEM_PLUS;
constexpr uint32_t TOAD_VK_OEM_COMMA = VK_OEM_COMMA;
constexpr uint32_t TOAD_VK_OEM_MINUS = VK_OEM_MINUS;
constexpr uint32_t TOAD_VK_OEM_PER   = VK_OEM_PERIOD;
constexpr uint32_t TOAD_VK_OEM_4     = VK_OEM_4;
constexpr uint32_t TOAD_VK_OEM_6     = VK_OEM_6;
#endif

// ============================================================================
// INPUT BINDING STRUCTURE
// ============================================================================
struct InputBinding {
    LogicalInput action{INPUT_NONE};
    uint32_t     primary_key{0};
    uint32_t     secondary_key{0};
    uint16_t     gamepad_mask{0};
    char         label[24]{};
};

inline const char* formatKeyName(uint32_t vk) {
    static char buf[16];
    if (vk >= 'A' && vk <= 'Z') {
        buf[0] = static_cast<char>(vk);
        buf[1] = '\0';
        return buf;
    }
    if (vk >= '0' && vk <= '9') {
        buf[0] = static_cast<char>(vk);
        buf[1] = '\0';
        return buf;
    }
    switch (vk) {
        case 0:                 return "---";
        case TOAD_VK_BACK:      return "BKSP";
        case TOAD_VK_TAB:       return "TAB";
        case TOAD_VK_RETURN:    return "ENTER";
        case TOAD_VK_SHIFT:     return "SHIFT";
        case TOAD_VK_CONTROL:   return "CTRL";
        case TOAD_VK_ESCAPE:    return "ESC";
        case TOAD_VK_SPACE:     return "SPACE";
        case TOAD_VK_PRIOR:     return "PGUP";
        case TOAD_VK_NEXT:      return "PGDN";
        case TOAD_VK_LEFT:      return "LEFT";
        case TOAD_VK_UP:        return "UP";
        case TOAD_VK_RIGHT:     return "RIGHT";
        case TOAD_VK_DOWN:      return "DOWN";
        case TOAD_VK_OEM_PLUS:  return "=";
        case TOAD_VK_OEM_COMMA: return ", (<)";
        case TOAD_VK_OEM_MINUS: return "-";
        case TOAD_VK_OEM_PER:   return ". (>)";
        case TOAD_VK_OEM_4:     return "[";
        case TOAD_VK_OEM_6:     return "]";
        default:
            std::snprintf(buf, sizeof(buf), "0x%02X", static_cast<unsigned int>(vk));
            return buf;
    }
}

inline const char* formatGamepadName(uint16_t mask) {
    switch (mask) {
        case 0x0001: return "D-UP";
        case 0x0002: return "D-DN";
        case 0x0004: return "D-LF";
        case 0x0008: return "D-RT";
        case 0x0010: return "START";
        case 0x0020: return "BACK";
        case 0x0100: return "LB";
        case 0x0200: return "RB";
        case 0x1000: return "BTN A";
        case 0x2000: return "BTN B";
        case 0x4000: return "BTN X";
        case 0x8000: return "BTN Y";
        default:     return "---";
    }
}

constexpr size_t TOTAL_CUSTOM_ACTIONS = 18;

enum PresetType : uint8_t {
    PRESET_M8_RUN = 0,
    PRESET_DESKTOP,
    PRESET_WASD,
    PRESET_VIM,
    PRESET_COUNT
};

// ============================================================================
// INPUT MAPPING TABLE & PRESET MANAGER
// ============================================================================
class InputMap {
public:
    InputMap() {
        loadM8RunPreset();
    }

    // --- PRESETS ---

    // 1. m8.run Preset: A & S as L & R page navigation, Z/X for Edit/Back, Space for Play
    void loadM8RunPreset() {
        activePreset_ = PRESET_M8_RUN;
        bindings_[0]  = { INPUT_PAGE_PREV,   'A',                TOAD_VK_PRIOR,  0x0100 /* LB */, "PAGE PREV (L)" };
        bindings_[1]  = { INPUT_PAGE_NEXT,   'S',                TOAD_VK_NEXT,   0x0200 /* RB */, "PAGE NEXT (R)" };
        bindings_[2]  = { INPUT_UP,          TOAD_VK_UP,         0,              0x0001 /* Up */, "CURSOR UP"     };
        bindings_[3]  = { INPUT_DOWN,        TOAD_VK_DOWN,       0,              0x0002 /* Dn */, "CURSOR DOWN"   };
        bindings_[4]  = { INPUT_LEFT,        TOAD_VK_LEFT,       0,              0x0004 /* Lf */, "CURSOR LEFT"   };
        bindings_[5]  = { INPUT_RIGHT,       TOAD_VK_RIGHT,      0,              0x0008 /* Rt */, "CURSOR RIGHT"  };
        bindings_[6]  = { INPUT_BTN_A,       'Z',                TOAD_VK_RETURN, 0x1000 /* A */,  "EDIT / CONFIRM"};
        bindings_[7]  = { INPUT_BTN_B,       'X',                TOAD_VK_ESCAPE, 0x2000 /* B */,  "BACK / CANCEL" };
        bindings_[8]  = { INPUT_BTN_OPT,     'C',                0,              0x4000 /* X */,  "OPTION / CTX"  };
        bindings_[9]  = { INPUT_BTN_EDIT,    'V',                TOAD_VK_TAB,    0x8000 /* Y */,  "AUDITION NOTE" };
        bindings_[10] = { INPUT_TRANSPORT,   TOAD_VK_SPACE,      0,              0x0010 /* Start*/,"PLAY / STOP"   };
        bindings_[11] = { INPUT_RECORD,      TOAD_VK_SHIFT,      'R',            0x0020 /* Back */,"RECORD ARM"   };
        bindings_[12] = { INPUT_VIEW_MOD,    0,                  TOAD_VK_CONTROL,0,               "VIEW MOD (L2)" };
        bindings_[13] = { INPUT_SUB_MOD,     0,                  0,              0,               "SUB MOD (R2)"  };
        bindings_[14] = { INPUT_OCTAVE_DOWN, TOAD_VK_OEM_COMMA,  TOAD_VK_OEM_MINUS, 0,           "OCTAVE DOWN"   };
        bindings_[15] = { INPUT_OCTAVE_UP,   TOAD_VK_OEM_PER,    TOAD_VK_OEM_PLUS,  0,           "OCTAVE UP"     };
        bindings_[16] = { INPUT_TRACK_MUTE,  0,                  0,              0,               "TRACK MUTE"    };
        bindings_[17] = { INPUT_TRACK_SOLO,  0,                  0,              0,               "TRACK SOLO"    };
    }

    // 2. Desktop Tracker Preset: Arrows, Enter/Esc, Tab, Space
    void loadDesktopPreset() {
        activePreset_ = PRESET_DESKTOP;
        bindings_[0]  = { INPUT_PAGE_PREV,   TOAD_VK_PRIOR,      0,              0x0100, "PAGE PREV (L)" };
        bindings_[1]  = { INPUT_PAGE_NEXT,   TOAD_VK_NEXT,       0,              0x0200, "PAGE NEXT (R)" };
        bindings_[2]  = { INPUT_UP,          TOAD_VK_UP,         0,              0x0001, "CURSOR UP"     };
        bindings_[3]  = { INPUT_DOWN,        TOAD_VK_DOWN,       0,              0x0002, "CURSOR DOWN"   };
        bindings_[4]  = { INPUT_LEFT,        TOAD_VK_LEFT,       0,              0x0004, "CURSOR LEFT"   };
        bindings_[5]  = { INPUT_RIGHT,       TOAD_VK_RIGHT,      0,              0x0008, "CURSOR RIGHT"  };
        bindings_[6]  = { INPUT_BTN_A,       TOAD_VK_RETURN,     'Z',            0x1000, "EDIT / CONFIRM"};
        bindings_[7]  = { INPUT_BTN_B,       TOAD_VK_ESCAPE,     'X',            0x2000, "BACK / CANCEL" };
        bindings_[8]  = { INPUT_BTN_OPT,     'C',                0,              0x4000, "OPTION / CTX"  };
        bindings_[9]  = { INPUT_BTN_EDIT,    TOAD_VK_TAB,        'V',            0x8000, "AUDITION NOTE" };
        bindings_[10] = { INPUT_TRANSPORT,   TOAD_VK_SPACE,      0,              0x0010, "PLAY / STOP"   };
        bindings_[11] = { INPUT_RECORD,      'R',                TOAD_VK_SHIFT,  0x0020, "RECORD ARM"   };
        bindings_[12] = { INPUT_VIEW_MOD,    0,                  0,              0,      "VIEW MOD (L2)" };
        bindings_[13] = { INPUT_SUB_MOD,     0,                  0,              0,      "SUB MOD (R2)"  };
        bindings_[14] = { INPUT_OCTAVE_DOWN, TOAD_VK_OEM_COMMA,  TOAD_VK_OEM_MINUS, 0,   "OCTAVE DOWN"   };
        bindings_[15] = { INPUT_OCTAVE_UP,   TOAD_VK_OEM_PER,    TOAD_VK_OEM_PLUS,  0,   "OCTAVE UP"     };
        bindings_[16] = { INPUT_TRACK_MUTE,  0,                  0,              0,      "TRACK MUTE"    };
        bindings_[17] = { INPUT_TRACK_SOLO,  0,                  0,              0,      "TRACK SOLO"    };
    }

    // 3. WASD Gamer Preset
    void loadWasdPreset() {
        activePreset_ = PRESET_WASD;
        bindings_[0]  = { INPUT_PAGE_PREV,   'Q',                0,              0x0100, "PAGE PREV (L)" };
        bindings_[1]  = { INPUT_PAGE_NEXT,   'E',                0,              0x0200, "PAGE NEXT (R)" };
        bindings_[2]  = { INPUT_UP,          'W',                0,              0x0001, "CURSOR UP"     };
        bindings_[3]  = { INPUT_DOWN,        'S',                0,              0x0002, "CURSOR DOWN"   };
        bindings_[4]  = { INPUT_LEFT,        'A',                0,              0x0004, "CURSOR LEFT"   };
        bindings_[5]  = { INPUT_RIGHT,       'D',                0,              0x0008, "CURSOR RIGHT"  };
        bindings_[6]  = { INPUT_BTN_A,       'J',                TOAD_VK_RETURN, 0x1000, "EDIT / CONFIRM"};
        bindings_[7]  = { INPUT_BTN_B,       'K',                TOAD_VK_ESCAPE, 0x2000, "BACK / CANCEL" };
        bindings_[8]  = { INPUT_BTN_OPT,     'U',                0,              0x4000, "OPTION / CTX"  };
        bindings_[9]  = { INPUT_BTN_EDIT,    'I',                TOAD_VK_TAB,    0x8000, "AUDITION NOTE" };
        bindings_[10] = { INPUT_TRANSPORT,   TOAD_VK_SPACE,      0,              0x0010, "PLAY / STOP"   };
        bindings_[11] = { INPUT_RECORD,      'R',                0,              0x0020, "RECORD ARM"   };
        bindings_[12] = { INPUT_VIEW_MOD,    0,                  0,              0,      "VIEW MOD (L2)" };
        bindings_[13] = { INPUT_SUB_MOD,     0,                  0,              0,      "SUB MOD (R2)"  };
        bindings_[14] = { INPUT_OCTAVE_DOWN, TOAD_VK_OEM_COMMA,  TOAD_VK_OEM_MINUS, 0,   "OCTAVE DOWN"   };
        bindings_[15] = { INPUT_OCTAVE_UP,   TOAD_VK_OEM_PER,    TOAD_VK_OEM_PLUS,  0,   "OCTAVE UP"     };
        bindings_[16] = { INPUT_TRACK_MUTE,  0,                  0,              0,      "TRACK MUTE"    };
        bindings_[17] = { INPUT_TRACK_SOLO,  0,                  0,              0,      "TRACK SOLO"    };
    }

    // 4. Vim HJKL Preset
    void loadVimPreset() {
        activePreset_ = PRESET_VIM;
        bindings_[0]  = { INPUT_PAGE_PREV,   TOAD_VK_OEM_4,      0,              0x0100, "PAGE PREV (L)" };
        bindings_[1]  = { INPUT_PAGE_NEXT,   TOAD_VK_OEM_6,      0,              0x0200, "PAGE NEXT (R)" };
        bindings_[2]  = { INPUT_UP,          'K',                0,              0x0001, "CURSOR UP"     };
        bindings_[3]  = { INPUT_DOWN,        'J',                0,              0x0002, "CURSOR DOWN"   };
        bindings_[4]  = { INPUT_LEFT,        'H',                0,              0x0004, "CURSOR LEFT"   };
        bindings_[5]  = { INPUT_RIGHT,       'L',                0,              0x0008, "CURSOR RIGHT"  };
        bindings_[6]  = { INPUT_BTN_A,       'Z',                TOAD_VK_RETURN, 0x1000, "EDIT / CONFIRM"};
        bindings_[7]  = { INPUT_BTN_B,       'X',                TOAD_VK_ESCAPE, 0x2000, "BACK / CANCEL" };
        bindings_[8]  = { INPUT_BTN_OPT,     'C',                0,              0x4000, "OPTION / CTX"  };
        bindings_[9]  = { INPUT_BTN_EDIT,    'V',                TOAD_VK_TAB,    0x8000, "AUDITION NOTE" };
        bindings_[10] = { INPUT_TRANSPORT,   TOAD_VK_SPACE,      0,              0x0010, "PLAY / STOP"   };
        bindings_[11] = { INPUT_RECORD,      'R',                0,              0x0020, "RECORD ARM"   };
        bindings_[12] = { INPUT_VIEW_MOD,    0,                  0,              0,      "VIEW MOD (L2)" };
        bindings_[13] = { INPUT_SUB_MOD,     0,                  0,              0,      "SUB MOD (R2)"  };
        bindings_[14] = { INPUT_OCTAVE_DOWN, TOAD_VK_OEM_COMMA,  TOAD_VK_OEM_MINUS, 0,   "OCTAVE DOWN"   };
        bindings_[15] = { INPUT_OCTAVE_UP,   TOAD_VK_OEM_PER,    TOAD_VK_OEM_PLUS,  0,   "OCTAVE UP"     };
        bindings_[16] = { INPUT_TRACK_MUTE,  0,                  0,              0,      "TRACK MUTE"    };
        bindings_[17] = { INPUT_TRACK_SOLO,  0,                  0,              0,      "TRACK SOLO"    };
    }

    // --- KEY RESOLUTION ---
    [[nodiscard]] LogicalInput resolveKey(uint32_t vk) const noexcept {
        for (size_t i = 0; i < TOTAL_CUSTOM_ACTIONS; ++i) {
            if (bindings_[i].primary_key == vk || bindings_[i].secondary_key == vk) {
                return bindings_[i].action;
            }
        }
        return INPUT_NONE;
    }

    [[nodiscard]] LogicalInput resolveGamepad(uint16_t buttons) const noexcept {
        for (size_t i = 0; i < TOTAL_CUSTOM_ACTIONS; ++i) {
            if (bindings_[i].gamepad_mask != 0 && (buttons & bindings_[i].gamepad_mask)) {
                return bindings_[i].action;
            }
        }
        return INPUT_NONE;
    }

    // --- RUNTIME REMAPPING ---
    void remapPrimaryKey(size_t index, uint32_t newKey) noexcept {
        if (index < TOTAL_CUSTOM_ACTIONS) {
            bindings_[index].primary_key = newKey;
        }
    }

    void remapGamepadButton(size_t index, uint16_t newButton) noexcept {
        if (index < TOTAL_CUSTOM_ACTIONS) {
            bindings_[index].gamepad_mask = newButton;
        }
    }

    // --- ACCESSORS ---
    [[nodiscard]] const std::array<InputBinding, TOTAL_CUSTOM_ACTIONS>& getBindings() const noexcept {
        return bindings_;
    }
    [[nodiscard]] PresetType getActivePreset() const noexcept { return activePreset_; }

    // --- VIRTUAL PIANO KEYBOARD ENGINE (Q..], 2..=, ,/<, ./>) ---
    // White keys: Q=C, W=D, E=E, R=F, T=G, Y=A, U=B, I=C+1, O=D+1, P=E+1, [=F+1, ]=G+1
    // Black keys: 2=C#, 3=D#, 5=F#, 6=G#, 7=A#, 9=C#+1, 0=D#+1, ==F#+1
    // Octave Shift: ,/< for Octave Down, ./> for Octave Up
    static int resolveVirtualPianoNote(uint32_t vk, int octaveOffset) noexcept {
        int semitone = -1;
        switch (vk) {
            // First Octave (C to B)
            case 'Q': semitone = 0;  break; // C
            case '2': semitone = 1;  break; // C#
            case 'W': semitone = 2;  break; // D
            case '3': semitone = 3;  break; // D#
            case 'E': semitone = 4;  break; // E
            case 'R': semitone = 5;  break; // F
            case '5': semitone = 6;  break; // F#
            case 'T': semitone = 7;  break; // G
            case '6': semitone = 8;  break; // G#
            case 'Y': semitone = 9;  break; // A
            case '7': semitone = 10; break; // A#
            case 'U': semitone = 11; break; // B

            // Second Octave (C+1 to G+1)
            case 'I':                semitone = 12; break; // C+1
            case '9':                semitone = 13; break; // C#+1
            case 'O':                semitone = 14; break; // D+1
            case '0':                semitone = 15; break; // D#+1
            case 'P':                semitone = 16; break; // E+1
            case TOAD_VK_OEM_4: // [
            case '[':                semitone = 17; break; // F+1
            case TOAD_VK_OEM_PLUS: // =
            case '=':                semitone = 18; break; // F#+1
            case TOAD_VK_OEM_6: // ]
            case ']':                semitone = 19; break; // G+1

            default: return -1;
        }

        // Base Octave 4 (Middle C = 60)
        int note = 60 + (octaveOffset * 12) + semitone;
        return std::max(0, std::min(127, note));
    }

    static bool isOctaveDownKey(uint32_t vk) noexcept {
        return (vk == TOAD_VK_OEM_COMMA || vk == ',');
    }

    static bool isOctaveUpKey(uint32_t vk) noexcept {
        return (vk == TOAD_VK_OEM_PER || vk == '.');
    }

    // --- PERSISTENCE ---
    bool saveToFile(const std::string& path) const {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) return false;

        uint32_t magic = 0x544D4150; // 'TMAP' (Toad Map)
        uint16_t version = 1;
        uint16_t count = static_cast<uint16_t>(TOTAL_CUSTOM_ACTIONS);

        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        out.write(reinterpret_cast<const char*>(&activePreset_), sizeof(activePreset_));

        for (size_t i = 0; i < TOTAL_CUSTOM_ACTIONS; ++i) {
            out.write(reinterpret_cast<const char*>(&bindings_[i]), sizeof(InputBinding));
        }

        return out.good();
    }

    bool loadFromFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;

        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t count = 0;

        in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic != 0x544D4150) return false;

        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        in.read(reinterpret_cast<char*>(&count), sizeof(count));
        if (count != TOTAL_CUSTOM_ACTIONS) return false;

        in.read(reinterpret_cast<char*>(&activePreset_), sizeof(activePreset_));

        for (size_t i = 0; i < TOTAL_CUSTOM_ACTIONS; ++i) {
            in.read(reinterpret_cast<char*>(&bindings_[i]), sizeof(InputBinding));
        }

        return in.good();
    }

private:
    std::array<InputBinding, TOTAL_CUSTOM_ACTIONS> bindings_{};
    PresetType activePreset_{PRESET_M8_RUN};
};

} // namespace toad
