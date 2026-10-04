#include "toad/format_bridge.h"
#include <cstring>
#include <algorithm>

namespace toad {

bool FormatBridge::slice64RowPattern(const ClassicPatternCell* pattern64Rows, size_t trackIndex,
                                     Chain& outChain, Phrase outPhrases[4]) {
    (void)trackIndex;
    if (!pattern64Rows) return false;

    // Reset chain steps
    for (size_t i = 0; i < 16; ++i) {
        outChain.steps[i].phrase_id = PHRASE_EMPTY;
        outChain.steps[i].transpose = 0;
    }

    // Split 64 rows into four 16-step phrases
    for (size_t p = 0; p < 4; ++p) {
        Phrase& phrase = outPhrases[p];
        outChain.steps[p].phrase_id = static_cast<uint8_t>(p);
        outChain.steps[p].transpose = 0;

        for (size_t s = 0; s < 16; ++s) {
            size_t srcRow = p * 16 + s;
            const ClassicPatternCell& src = pattern64Rows[srcRow];
            PhraseStep& dst = phrase.steps[s];

            dst.note = src.note;
            dst.instrument = src.instrument;
            dst.volume = (src.volume != 0) ? src.volume : 0xFF;

            // Effect conversion
            if (src.effect_cmd != 0) {
                dst.fx[0].cmd = CMD_FCUT;
                dst.fx[0].val = src.effect_val;
            } else {
                dst.fx[0].cmd = CMD_NONE;
                dst.fx[0].val = 0;
            }
            dst.fx[1].cmd = CMD_NONE;
            dst.fx[1].val = 0;
            dst.fx[2].cmd = CMD_NONE;
            dst.fx[2].val = 0;
        }
    }

    return true;
}

bool FormatBridge::translateEnvelopeToTable(const EnvelopePoint* points, size_t pointCount,
                                            bool loopSustain, uint8_t sustainPointIndex,
                                            Table& outTable) {
    if (!points || pointCount < 2) return false;

    outTable.speed = 1;
    outTable.loop = loopSustain;

    for (size_t r = 0; r < 16; ++r) {
        float t = static_cast<float>(r) / 15.0f;
        float val = 0.0f;

        // Piecewise linear interpolation between points
        if (t <= points[0].time_norm) {
            val = points[0].value_norm;
        } else if (t >= points[pointCount - 1].time_norm) {
            val = points[pointCount - 1].value_norm;
        } else {
            for (size_t i = 0; i < pointCount - 1; ++i) {
                if (t >= points[i].time_norm && t <= points[i + 1].time_norm) {
                    float segmentLen = points[i + 1].time_norm - points[i].time_norm;
                    float frac = (segmentLen > 1e-6f) ? (t - points[i].time_norm) / segmentLen : 0.0f;
                    val = points[i].value_norm + frac * (points[i + 1].value_norm - points[i].value_norm);
                    break;
                }
            }
        }

        val = std::clamp(val, 0.0f, 1.0f);
        outTable.rows[r].transpose = 0;
        outTable.rows[r].volume = static_cast<uint8_t>(val * 255.0f);
        outTable.rows[r].cmd1 = CMD_NONE;
        outTable.rows[r].val1 = 0;
        outTable.rows[r].cmd2 = CMD_NONE;
        outTable.rows[r].val2 = 0;
    }

    if (loopSustain) {
        outTable.rows[15].cmd1 = CMD_HOP_;
        outTable.rows[15].val1 = std::min(static_cast<uint8_t>(15), sustainPointIndex);
    }

    return true;
}

bool FormatBridge::compactSparseChannels(const Phrase& channelA, const Phrase& channelB, Phrase& outMerged) {
    // Check for collisions
    for (size_t s = 0; s < 16; ++s) {
        if (channelA.steps[s].note != NOTE_EMPTY && channelB.steps[s].note != NOTE_EMPTY) {
            return false; // Collision detected, cannot compact safely
        }
    }

    // Merge non-overlapping steps
    for (size_t s = 0; s < 16; ++s) {
        if (channelA.steps[s].note != NOTE_EMPTY) {
            outMerged.steps[s] = channelA.steps[s];
        } else if (channelB.steps[s].note != NOTE_EMPTY) {
            outMerged.steps[s] = channelB.steps[s];
        } else {
            outMerged.steps[s].note = NOTE_EMPTY;
            outMerged.steps[s].instrument = 0;
            outMerged.steps[s].volume = 0;
            for (int f = 0; f < 3; ++f) {
                outMerged.steps[s].fx[f].cmd = CMD_NONE;
                outMerged.steps[s].fx[f].val = 0;
            }
        }
    }

    return true;
}

TrackerCommand FormatBridge::m8CommandToToad(M8Command m8Cmd) {
    switch (m8Cmd) {
        case M8_CMD_CUT: return CMD_FCUT;
        case M8_CMD_RES: return CMD_FRES;
        case M8_CMD_DRV: return CMD_FDRV;
        case M8_CMD_PWM: return CMD_PWM_;
        case M8_CMD_HOP: return CMD_HOP_;
        case M8_CMD_TBL: return CMD_ATBL;
        default:         return CMD_NONE;
    }
}

M8Command FormatBridge::toadCommandToM8(TrackerCommand toadCmd) {
    switch (toadCmd) {
        case CMD_FCUT: return M8_CMD_CUT;
        case CMD_FRES: return M8_CMD_RES;
        case CMD_FDRV: return M8_CMD_DRV;
        case CMD_PWM_: return M8_CMD_PWM;
        case CMD_HOP_: return M8_CMD_HOP;
        case CMD_ATBL: return M8_CMD_TBL;
        default:       return M8_CMD_NONE;
    }
}

bool FormatBridge::exportToM8Format(const Song& toadSong, std::vector<uint8_t>& outM8Bytes) {
    M8SongHeader header;
    header.tempo_bpm = toadSong.bpm;

    size_t totalSize = sizeof(M8SongHeader) + sizeof(toadSong.rows) + sizeof(toadSong.chains) + sizeof(toadSong.phrases);
    outM8Bytes.resize(totalSize);

    size_t offset = 0;
    std::memcpy(outM8Bytes.data() + offset, &header, sizeof(M8SongHeader));
    offset += sizeof(M8SongHeader);

    std::memcpy(outM8Bytes.data() + offset, toadSong.rows, sizeof(toadSong.rows));
    offset += sizeof(toadSong.rows);

    std::memcpy(outM8Bytes.data() + offset, toadSong.chains, sizeof(toadSong.chains));
    offset += sizeof(toadSong.chains);

    std::memcpy(outM8Bytes.data() + offset, toadSong.phrases, sizeof(toadSong.phrases));

    return true;
}

bool FormatBridge::importFromM8Format(const uint8_t* m8Bytes, size_t size, Song& outToadSong) {
    if (!m8Bytes || size < sizeof(M8SongHeader)) return false;

    M8SongHeader header;
    std::memcpy(&header, m8Bytes, sizeof(M8SongHeader));

    if (std::memcmp(header.magic, "M8VERSION", 9) != 0) return false;

    outToadSong.bpm = header.tempo_bpm;
    size_t offset = sizeof(M8SongHeader);

    if (size >= offset + sizeof(outToadSong.rows)) {
        std::memcpy(outToadSong.rows, m8Bytes + offset, sizeof(outToadSong.rows));
        offset += sizeof(outToadSong.rows);
    }

    if (size >= offset + sizeof(outToadSong.chains)) {
        std::memcpy(outToadSong.chains, m8Bytes + offset, sizeof(outToadSong.chains));
        offset += sizeof(outToadSong.chains);
    }

    if (size >= offset + sizeof(outToadSong.phrases)) {
        std::memcpy(outToadSong.phrases, m8Bytes + offset, sizeof(outToadSong.phrases));
    }

    return true;
}

} // namespace toad
