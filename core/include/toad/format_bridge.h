#pragma once

#include "toad/types.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace toad {

// ============================================================================
// DIRTYWAVE M8 EFFECT COMMAND ENUMERATION
// ============================================================================
enum M8Command : uint8_t {
    M8_CMD_NONE = 0x00,
    M8_CMD_VOL  = 0x01,
    M8_CMD_PIT  = 0x02,
    M8_CMD_CUT  = 0x03,
    M8_CMD_RES  = 0x04,
    M8_CMD_DRV  = 0x05,
    M8_CMD_PWM  = 0x06,
    M8_CMD_HOP  = 0x07,
    M8_CMD_TBL  = 0x08,
    M8_CMD_KIL  = 0x09,
    M8_CMD_DEL  = 0x0A
};

// ============================================================================
// 64-ROW CLASSIC TRACKER PATTERN CELL (XM / IT / MOD)
// ============================================================================
struct ClassicPatternCell {
    uint8_t note{NOTE_EMPTY};
    uint8_t instrument{0};
    uint8_t volume{0};
    uint8_t effect_cmd{0};
    uint8_t effect_val{0};
};

// ============================================================================
// FORMAT BRIDGE & TRANSPILATION ENGINE (Section 8.2)
// ============================================================================
class FormatBridge {
public:
    // --- PHRASE SLICING (Section 8.2) ---
    // Splits a 64-row classic pattern into 4 contiguous 16-step phrases on a chain
    static bool slice64RowPattern(const ClassicPatternCell* pattern64Rows, size_t trackIndex,
                                  Chain& outChain, Phrase outPhrases[4]);

    // --- ENVELOPE TRANSLATION (Section 8.2) ---
    // Samples a multi-point ADSR / envelope curve into a 16-row Primary Table with CMD_HOP_
    struct EnvelopePoint {
        float time_norm{0.0f};  // 0.0 .. 1.0
        float value_norm{0.0f}; // 0.0 .. 1.0
    };

    static bool translateEnvelopeToTable(const EnvelopePoint* points, size_t pointCount,
                                        bool loopSustain, uint8_t sustainPointIndex,
                                        Table& outTable);

    // --- CHANNEL COMPACTION (Section 8.2) ---
    // Merges two sparse non-overlapping channels into a single track
    static bool compactSparseChannels(const Phrase& channelA, const Phrase& channelB, Phrase& outMerged);

    // --- DIRTYWAVE M8 BIDIRECTIONAL MAPPING ---
    static TrackerCommand m8CommandToToad(M8Command m8Cmd);
    static M8Command toadCommandToM8(TrackerCommand toadCmd);

    // Ingests / Exports M8 Song Structure
    struct M8SongHeader {
        char magic[12]{"M8VERSION"};
        uint8_t version_major{1};
        uint8_t version_minor{0};
        float tempo_bpm{120.0f};
    };

    static bool exportToM8Format(const Song& toadSong, std::vector<uint8_t>& outM8Bytes);
    static bool importFromM8Format(const uint8_t* m8Bytes, size_t size, Song& outToadSong);
};

} // namespace toad
