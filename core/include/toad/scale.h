#pragma once

#include "types.h"

namespace toad {

inline int8_t quantizeToScale(const ScaleDefinition& scale, int8_t raw_note) {
    if (!scale.enabled || scale.note_mask == 0x0FFF || scale.note_mask == 0) {
        return raw_note;
    }

    int8_t octave = raw_note / 12;
    int8_t semitone = raw_note % 12;
    if (semitone < 0) {
        semitone += 12;
        octave -= 1;
    }

    int8_t rel_semitone = (semitone - scale.root_note + 12) % 12;
    if ((scale.note_mask >> rel_semitone) & 1) {
        return raw_note;
    }

    int8_t lower_dist = 0;
    int8_t higher_dist = 0;
    for (int d = 1; d <= 6; ++d) {
        if (lower_dist == 0 && ((scale.note_mask >> ((rel_semitone - d + 12) % 12)) & 1)) {
            lower_dist = static_cast<int8_t>(d);
        }
        if (higher_dist == 0 && ((scale.note_mask >> ((rel_semitone + d) % 12)) & 1)) {
            higher_dist = static_cast<int8_t>(d);
        }
        if (lower_dist != 0 && higher_dist != 0) break;
    }

    int8_t offset = 0;
    if (scale.snap_mode == SNAP_DOWN) {
        offset = -lower_dist;
    } else if (scale.snap_mode == SNAP_UP) {
        offset = higher_dist;
    } else {
        offset = (lower_dist <= higher_dist) ? -lower_dist : higher_dist;
    }

    return static_cast<int8_t>((octave * 12) + scale.root_note + rel_semitone + offset);
}

} // namespace toad
