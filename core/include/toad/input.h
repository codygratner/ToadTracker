#pragma once

#include <cstdint>

namespace toad {

// ============================================================================
// LOGICAL INPUT CODES (M8 8-Button Standard + Extended Steam Deck / Handheld)
// ============================================================================
enum LogicalInput : uint8_t {
    INPUT_NONE = 0,

    // M8 / TBD-16 Baseline Controls
    INPUT_PAGE_PREV,    // L1 (Left Bumper) -> View Stack Previous
    INPUT_PAGE_NEXT,    // R1 (Right Bumper) -> View Stack Next
    INPUT_UP,           // D-Pad Up / Encoder 1 Up
    INPUT_DOWN,         // D-Pad Down / Encoder 1 Down
    INPUT_LEFT,         // D-Pad Left / Encoder 2 Left
    INPUT_RIGHT,        // D-Pad Right / Encoder 2 Right
    INPUT_BTN_A,        // Button A / Enc 3 Click -> Value Edit / Confirm
    INPUT_BTN_B,        // Button B / Back Switch -> Cancel / Stop / Value Dec
    INPUT_BTN_OPT,      // Button X / Screen Select -> Option / Context Menu
    INPUT_BTN_EDIT,     // Button Y / Step Pad Tap -> Audition Note / Preview
    INPUT_TRANSPORT,    // Start Button / Play Switch -> Play / Pause
    INPUT_RECORD,       // Select Button / Record Switch -> Arm Record / Param Lock

    // Modifier Combos (L2 / R2 Triggers)
    INPUT_VIEW_MOD,     // L2 Trigger (Quick View Toggle: Song <-> Phrase)
    INPUT_SUB_MOD,      // R2 Trigger (Quick Sub-View Jump: Synth <-> Table <-> Pool)

    // Handheld / Steam Deck Extensions
    INPUT_OCTAVE_DOWN,  // L4 Rear Grip -> Octave Down (-12 semitones)
    INPUT_OCTAVE_UP,    // L5 Rear Grip -> Octave Up (+12 semitones)
    INPUT_TRACK_MUTE,   // R4 Rear Grip -> Mute / Unmute Active Track
    INPUT_TRACK_SOLO,   // R5 Rear Grip -> Solo / Unsolo Active Track
    INPUT_JOG_CW,       // Trackpad Jog Clockwise / Value Scrub Up
    INPUT_JOG_CCW       // Trackpad Jog Counter-Clockwise / Value Scrub Down
};

// ============================================================================
// INPUT EVENT STRUCTURE
// ============================================================================
struct InputEvent {
    LogicalInput input{INPUT_NONE};
    bool pressed{false};          // true = key/button down, false = release
    bool modifier_view{false};    // L2 held
    bool modifier_sub{false};     // R2 held
    int16_t jog_delta{0};         // Relative jog steps for rotary trackpad
    uint32_t timestamp_ms{0};     // Milliseconds from boot/start
};

} // namespace toad
