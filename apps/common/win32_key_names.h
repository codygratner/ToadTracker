#pragma once

#include <cstdint>
#include <cstdio>

namespace toad {

inline const char* getKeyName(uint32_t vk) {
    static char buf[16];

    // Single ASCII Characters
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
        case 0:           return "---";
        case 0x08:        return "BACKSP";
        case 0x09:        return "TAB";
        case 0x0D:        return "ENTER";
        case 0x10:        return "SHIFT";
        case 0x11:        return "CTRL";
        case 0x12:        return "ALT";
        case 0x1B:        return "ESC";
        case 0x20:        return "SPACE";
        case 0x21:        return "PG UP";
        case 0x22:        return "PG DN";
        case 0x25:        return "LEFT";
        case 0x26:        return "UP";
        case 0x27:        return "RIGHT";
        case 0x28:        return "DOWN";
        case 0xBB: case '=': return "=";
        case 0xBC: case ',': return ", (<)";
        case 0xBD: case '-': return "-";
        case 0xBE: case '.': return ". (>)";
        case 0xDB: case '[': return "[";
        case 0xDD: case ']': return "]";
        case 0x70:        return "F1";
        case 0x71:        return "F2";
        case 0x72:        return "F3";
        case 0x73:        return "F4";
        case 0x74:        return "F5";
        case 0x75:        return "F6";
        case 0x76:        return "F7";
        case 0x77:        return "F8";
        case 0x78:        return "F9";
        case 0x79:        return "F10";
        case 0x7A:        return "F11";
        case 0x7B:        return "F12";
        default:
            std::snprintf(buf, sizeof(buf), "0x%02X", static_cast<unsigned int>(vk));
            return buf;
    }
}

inline const char* getGamepadButtonName(uint16_t mask) {
    switch (mask) {
        case 0x0001: return "DPAD UP";
        case 0x0002: return "DPAD DN";
        case 0x0004: return "DPAD LF";
        case 0x0008: return "DPAD RT";
        case 0x0010: return "START";
        case 0x0020: return "BACK";
        case 0x0040: return "LS CLICK";
        case 0x0080: return "RS CLICK";
        case 0x0100: return "LB (L1)";
        case 0x0200: return "RB (R1)";
        case 0x1000: return "BTN A";
        case 0x2000: return "BTN B";
        case 0x4000: return "BTN X";
        case 0x8000: return "BTN Y";
        default:     return "---";
    }
}

} // namespace toad
