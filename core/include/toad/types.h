#pragma once

#include <cstdint>
#include <cstddef>

namespace toad {

// --- CORE SYSTEM LIMITS ---
constexpr size_t MAX_TRACKS        = 8;
constexpr size_t PHRASE_STEPS      = 16;
constexpr size_t TABLE_ROWS        = 16;
constexpr size_t TOTAL_CHAINS      = 255;
constexpr size_t TOTAL_PHRASES     = 255;
constexpr size_t TOTAL_TABLES      = 64;
constexpr size_t TOTAL_INSTRUMENTS = 64;
constexpr size_t MAX_POOL_SLOTS    = 8;
constexpr size_t TOTAL_SONG_ROWS   = 256;

// --- SENTINEL VALUE CONSTANTS ---
constexpr uint8_t NOTE_EMPTY       = 0xFF; // 00..7F valid note, 0xFF = Empty / No Trigger
constexpr uint8_t INST_EMPTY       = 0xFF; // 00..3F valid instrument, 0xFF = Empty
constexpr uint8_t CHAIN_EMPTY      = 0xFF; // 00..FE valid chain ID, 0xFF = Empty track slot
constexpr uint8_t PHRASE_EMPTY     = 0xFF; // 00..FE valid phrase ID, 0xFF = End of Chain / Empty
constexpr uint8_t TABLE_EMPTY      = 0xFF; // 00..3F valid table ID, 0xFF = None

// --- COMMAND ENUMS (Strict Hex Values) ---
enum TrackerCommand : uint8_t {
    CMD_NONE = 0x00,
    CMD_ARPG = 0x01, // Arpeggio offset (x = semitone 1, y = semitone 2)
    CMD_FCUT = 0x02, // Universal Filter Cutoff (00..FF)
    CMD_FRES = 0x03, // Universal Filter Resonance (00..FF)
    CMD_FOLD = 0x04, // Wavefolder Drive Amount (00..FF)
    CMD_PWM_ = 0x05, // Pulse Width Modulation (00..FF)
    CMD_DISP = 0x06, // Allpass Disperser Frequency (00..FF)
    CMD_WPOS = 0x07, // Wavetable Frame Position Scan (00..FF)
    CMD_WAMT = 0x08, // Wavetable Warp Intensity (00..FF)
    CMD_FDRV = 0x09, // Filter Saturation Drive (00..FF)
    CMD_SMP1 = 0x0A, // Sample Start Offset Point (00..FF)
    CMD_SLOP = 0x0B, // Sample Loop Mode (00 = Fwd, 01 = PingPong, 02 = Off)
    CMD_ATBL = 0x10, // Trigger Aux Table (Val = Table ID 00..3F)
    CMD_HOP_ = 0x20, // Table Loop/Jump Target (Val = Step 00..0F)
    CMD_MCC1 = 0x30, // Send MIDI Macro CC 1 (00..7F)
    CMD_MCC2 = 0x31, // Send MIDI Macro CC 2 (00..7F)
    CMD_MCC3 = 0x32, // Send MIDI Macro CC 3 (00..7F)
    CMD_MCC4 = 0x33, // Send MIDI Macro CC 4 (00..7F)
    CMD_MPCH = 0x34, // Send MIDI Program Change (00..7F)
    CMD_PBND = 0x35  // Send MIDI Pitch Bend (-64..+63 -> 14-bit)
};

// --- SEQUENCER ROW UNITS ---
struct StepEffect {
    TrackerCommand cmd{CMD_NONE};
    uint8_t        val{0x00};
};

struct PhraseStep {
    uint8_t    note{NOTE_EMPTY};       // 00..7F (0xFF = Empty/No Trigger)
    uint8_t    instrument{INST_EMPTY}; // 00..3F (0xFF = Empty)
    uint8_t    volume{0x00};           // 00..FF
    StepEffect fx[3]{};                // 3 Stackable FX columns per step (M8 style)
};

struct Phrase {
    PhraseStep steps[PHRASE_STEPS]{};
};

struct ChainStep {
    uint8_t phrase_id{PHRASE_EMPTY};   // 00..FE (0xFF = End of Chain)
    int8_t  transpose{0};              // Signed semitone transposition (-7F..7F)
};

struct Chain {
    ChainStep steps[16]{};
};

struct SongRow {
    uint8_t chain_ids[MAX_TRACKS]{
        CHAIN_EMPTY, CHAIN_EMPTY, CHAIN_EMPTY, CHAIN_EMPTY,
        CHAIN_EMPTY, CHAIN_EMPTY, CHAIN_EMPTY, CHAIN_EMPTY
    }; // Chain ID per track (0xFF = Empty)
};

// --- TABLE MODULATION UNITS ---
struct TableRow {
    int8_t         transpose{0}; // Semitone offset (-80..7F)
    uint8_t        volume{0};    // Volume scaling (00 = ignore, 01..FF)
    TrackerCommand cmd1{CMD_NONE};
    uint8_t        val1{0};
    TrackerCommand cmd2{CMD_NONE};
    uint8_t        val2{0};
};

struct Table {
    TableRow rows[TABLE_ROWS]{};
    uint8_t  speed{1};    // Ticks per row (01 = audio-rate tick, 06 = standard)
    bool     loop{false}; // Loop execution flag
};

struct TablePlayer {
    uint8_t table_id{0};
    int8_t  current_row{-1};
    uint8_t tick_counter{0};
    uint8_t speed{1};
    bool    active{false};

    inline void trigger(uint8_t id, uint8_t default_speed = 1) {
        table_id = id;
        current_row = 0;
        tick_counter = 0;
        speed = default_speed;
        active = true;
    }

    inline void stop() {
        active = false;
        current_row = -1;
        tick_counter = 0;
    }

    // Advances the table runner by one tick.
    // Handles tick subdivision, row progression, CMD_HOP_ jumps, and loop bounds.
    // Returns true when a row transition occurs.
    inline bool tick(const Table& table) {
        if (!active) return false;

        tick_counter++;
        if (tick_counter < speed) {
            return false;
        }
        tick_counter = 0;

        int next_row = current_row + 1;
        if (current_row >= 0 && current_row < static_cast<int8_t>(TABLE_ROWS)) {
            const auto& row = table.rows[current_row];
            if (row.cmd1 == CMD_HOP_) {
                next_row = row.val1 & 0x0F;
            } else if (row.cmd2 == CMD_HOP_) {
                next_row = row.val2 & 0x0F;
            }
        }

        if (next_row >= static_cast<int8_t>(TABLE_ROWS)) {
            if (table.loop) {
                next_row = 0;
            } else {
                stop();
                return false;
            }
        }

        current_row = static_cast<int8_t>(next_row);
        return true;
    }
};

// --- PROBABILISTIC NOTE POOL (RENOISE Yxx) ---
enum PoolMode : uint8_t {
    POOL_OFF          = 0x00,
    POOL_WEIGHTED_RND = 0x01,
    POOL_CYCLE        = 0x02,
    POOL_SHUFFLE      = 0x03
};

struct PoolSlot {
    int8_t  semitone_offset{0};   // Relative semitone (-24..+24)
    uint8_t weight{0};            // 00 = Disabled, 01..FF relative weight
    uint8_t velocity_scale{0xFF}; // Dynamics scaling (00..FF)
};

struct InstrumentNotePool {
    PoolMode mode{POOL_OFF};
    uint8_t  slot_count{0};
    PoolSlot slots[MAX_POOL_SLOTS]{};
    uint8_t  last_selected{0};
};

// --- SCALE QUANTIZATION ENGINE ---
enum SnapMode : uint8_t {
    SNAP_DOWN    = 0x00,
    SNAP_UP      = 0x01,
    SNAP_NEAREST = 0x02
};

struct ScaleDefinition {
    char     name[12]{};
    uint8_t  root_note{0};        // 00 = C ... 0B = B
    uint16_t note_mask{0x0FFF};   // 12-bit active semitone mask (Bit 0 = Root)
    SnapMode snap_mode{SNAP_NEAREST};
    bool     enabled{false};
};

// --- UNIFIED FILTER & SATURATION DEFINITIONS ---
enum FilterType : uint8_t {
    FLT_BYPASS      = 0x00,
    FLT_LP12_SVF    = 0x01,
    FLT_LP24_LADDER = 0x02,
    FLT_HP12_SVF    = 0x03,
    FLT_BP12_SVF    = 0x04,
    FLT_NOTCH_SVF   = 0x05,
    FLT_PEAK_SVF    = 0x06,
    FLT_COMB_POS    = 0x07,
    FLT_COMB_NEG    = 0x08
};

enum WarpMode : uint8_t {
    WARP_OFF     = 0x00,
    WARP_PWM     = 0x01,
    WARP_SYNC    = 0x02,
    WARP_BEND    = 0x03,
    WARP_FOLD    = 0x04,
    WARP_FORMANT = 0x05,
    WARP_BITCR   = 0x06
};

// --- INSTRUMENT DEFINITIONS ---
enum InstrumentType : uint8_t {
    INST_TYPE_INTERNAL_SYNTH  = 0x00,
    INST_TYPE_WAVETABLE       = 0x01,
    INST_TYPE_SAMPLE_PLAYER   = 0x02,
    INST_TYPE_SF2_MULTISAMPLE = 0x03,
    INST_TYPE_MIDI_OUT        = 0x04
};

struct MidiConfig {
    uint8_t midi_channel{0};
    uint8_t default_program{0};
    uint8_t bank_msb{0};
    uint8_t bank_lsb{0};
    uint8_t cc_assignments[4]{0};
};

struct SynthConfig {
    float   pulse_width{0.5f};
    float   fold_drive{0.0f};
    float   disperser_freq{1000.0f};
    uint8_t disperser_stages{1};
};

struct WavetableConfig {
    uint8_t  table_index{0};
    float    position{0.0f};
    WarpMode warp_mode{WARP_OFF};
    float    warp_amount{0.0f};
};

struct Instrument {
    char               name[12]{};
    InstrumentType     type{INST_TYPE_INTERNAL_SYNTH};
    uint8_t            table_id{TABLE_EMPTY};
    InstrumentNotePool note_pool{};
    ScaleDefinition    scale{};
    FilterType         filter_type{FLT_BYPASS};
    float              filter_cutoff{20000.0f};
    float              filter_resonance{0.0f};
    float              drive_amount{0.0f};

    union {
        SynthConfig     synth;
        WavetableConfig wavetable;
        MidiConfig      midi;
    };

    Instrument() : synth{} {}
};

// --- MASTER SONG STRUCTURE ---
struct Song {
    char            name[32]{"UNTITLED"};
    float           bpm{120.0f};
    uint8_t         groove{6}; // Default 6 ticks per step
    SongRow         rows[TOTAL_SONG_ROWS]{};
    Chain           chains[TOTAL_CHAINS]{};
    Phrase          phrases[TOTAL_PHRASES]{};
    Table           tables[TOTAL_TABLES]{};
    Instrument      instruments[TOTAL_INSTRUMENTS]{};
    ScaleDefinition master_scale{};
};

} // namespace toad
