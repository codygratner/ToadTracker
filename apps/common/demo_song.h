#pragma once

#include <toad/types.h>
#include <toad/engine.h>

namespace toad {

inline Song createDemoSong() {
    Song song{};
    song.bpm = 128.0f;
    song.groove = 6;
    song.master_scale.enabled = true;
    song.master_scale.root_note = 0;       // C
    song.master_scale.note_mask = 0x05AB;  // C Minor: 1 0 1 1 0 1 0 1 1 0 1 0
    song.master_scale.snap_mode = SNAP_NEAREST;

    // ------------------------------------------------------------------------
    // INSTRUMENT 0: Acid Bass (Internal Synth + Ladder Filter + Disperser)
    // ------------------------------------------------------------------------
    song.instruments[0].type = INST_TYPE_INTERNAL_SYNTH;
    song.instruments[0].synth.pulse_width = 0.5f;
    song.instruments[0].synth.fold_drive = 0.35f;
    song.instruments[0].synth.disperser_stages = 4;
    song.instruments[0].synth.disperser_freq = 1200.0f;
    song.instruments[0].filter_type = FLT_LP24_LADDER;
    song.instruments[0].filter_cutoff = 1800.0f;
    song.instruments[0].filter_resonance = 0.65f;
    song.instruments[0].table_id = 0; // Primary Table 0

    // Table 0: Bass Filter Sweep & Accent Envelope
    song.tables[0].speed = 1;
    song.tables[0].loop = true;
    for (int r = 0; r < 16; ++r) {
        song.tables[0].rows[r].transpose = 0;
        song.tables[0].rows[r].volume = static_cast<uint8_t>(255 - r * 10);
        song.tables[0].rows[r].cmd1 = CMD_FCUT;
        song.tables[0].rows[r].val1 = static_cast<uint8_t>(0xE0 - r * 8); // Cutoff sweep down
        song.tables[0].rows[r].cmd2 = (r == 15) ? CMD_HOP_ : CMD_NONE;
        song.tables[0].rows[r].val2 = (r == 15) ? 4 : 0; // Loop rows 4..15
    }

    // ------------------------------------------------------------------------
    // INSTRUMENT 1: Cyber Wavetable Lead (Wavetable + Asymmetric Bend Warp)
    // ------------------------------------------------------------------------
    song.instruments[1].type = INST_TYPE_WAVETABLE;
    song.instruments[1].wavetable.warp_mode = WARP_BEND;
    song.instruments[1].wavetable.warp_amount = 0.60f;
    song.instruments[1].wavetable.position = 12.0f;
    song.instruments[1].filter_type = FLT_LP12_SVF;
    song.instruments[1].filter_cutoff = 4500.0f;
    song.instruments[1].filter_resonance = 0.25f;
    song.instruments[1].table_id = 1;

    // Table 1: Wavetable Position LFO & Pitch Vibrato
    song.tables[1].speed = 2;
    song.tables[1].loop = true;
    for (int r = 0; r < 8; ++r) {
        song.tables[1].rows[r].transpose = (r % 2 == 0) ? 0 : 1;
        song.tables[1].rows[r].volume = 240;
        song.tables[1].rows[r].cmd1 = CMD_WAMT;
        song.tables[1].rows[r].val1 = static_cast<uint8_t>(0x40 + (r * 12));
        song.tables[1].rows[r].cmd2 = (r == 7) ? CMD_HOP_ : CMD_NONE;
        song.tables[1].rows[r].val2 = 0;
    }

    // ------------------------------------------------------------------------
    // INSTRUMENT 2: Punchy 808 Kick & Transient Bass (Fast Pitch Drop)
    // ------------------------------------------------------------------------
    song.instruments[2].type = INST_TYPE_INTERNAL_SYNTH;
    song.instruments[2].synth.pulse_width = 0.5f;
    song.instruments[2].synth.fold_drive = 0.2f;
    song.instruments[2].filter_type = FLT_BYPASS;
    song.instruments[2].table_id = 2;

    // Table 2: Fast Pitch Drop for Punchy Kick
    song.tables[2].speed = 1;
    song.tables[2].loop = false;
    song.tables[2].rows[0] = { 24, 0xFF, CMD_NONE, 0, CMD_NONE, 0 }; // +24 semitones spike
    song.tables[2].rows[1] = { 12, 0xF0, CMD_NONE, 0, CMD_NONE, 0 }; // +12 semitones
    song.tables[2].rows[2] = {  5, 0xE0, CMD_NONE, 0, CMD_NONE, 0 };
    song.tables[2].rows[3] = {  0, 0xD0, CMD_NONE, 0, CMD_NONE, 0 };
    song.tables[2].rows[4] = {  0, 0xB0, CMD_NONE, 0, CMD_NONE, 0 };
    song.tables[2].rows[5] = {  0, 0x80, CMD_NONE, 0, CMD_NONE, 0 };
    song.tables[2].rows[6] = {  0, 0x40, CMD_NONE, 0, CMD_NONE, 0 };
    song.tables[2].rows[7] = {  0, 0x00, CMD_HOP_, 7, CMD_NONE, 0 };

    // ------------------------------------------------------------------------
    // INSTRUMENT 3: Metallic Snare / Hi-Hat
    // ------------------------------------------------------------------------
    song.instruments[3].type = INST_TYPE_INTERNAL_SYNTH;
    song.instruments[3].synth.pulse_width = 0.15f;
    song.instruments[3].synth.fold_drive = 0.85f;
    song.instruments[3].synth.disperser_stages = 8;
    song.instruments[3].synth.disperser_freq = 6000.0f;
    song.instruments[3].filter_type = FLT_HP12_SVF;
    song.instruments[3].filter_cutoff = 8000.0f;
    song.instruments[3].filter_resonance = 0.4f;

    // ------------------------------------------------------------------------
    // PHRASES:
    // Phrase 0: Acid Bass Line (C Minor: C2, Eb2, F2, G2, Bb2, C3)
    // ------------------------------------------------------------------------
    uint8_t c2 = 36, eb2 = 39, f2 = 41, g2 = 43, bb2 = 46, c3 = 48;
    song.phrases[0].steps[0]  = { c2,  0, 0xFF, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[0].steps[2]  = { c2,  0, 0xD0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[0].steps[3]  = { eb2, 0, 0xF0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[0].steps[4]  = { c2,  0, 0xE0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[0].steps[6]  = { f2,  0, 0xD0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[0].steps[7]  = { g2,  0, 0xFF, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[0].steps[8]  = { c2,  0, 0xFF, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[0].steps[10] = { bb2, 0, 0xE0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[0].steps[12] = { c3,  0, 0xFF, {{CMD_FOLD, 0x80}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[0].steps[14] = { g2,  0, 0xC0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[0].steps[15] = { eb2, 0, 0xD0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };

    // Phrase 1: Kick & Snare Beat
    // Step 0: Kick, Step 4: Snare, Step 8: Kick, Step 12: Snare
    song.phrases[1].steps[0]  = { 36, 2, 0xFF, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[1].steps[4]  = { 60, 3, 0xD0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[1].steps[8]  = { 36, 2, 0xFF, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[1].steps[10] = { 36, 2, 0xB0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[1].steps[12] = { 60, 3, 0xE0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[1].steps[14] = { 60, 3, 0x90, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };

    // Phrase 2: Wavetable Arp Melody (C4, G4, Eb5, Bb4, C5, G5, F5, D5)
    song.phrases[2].steps[0]  = { 60, 1, 0xD0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[2].steps[2]  = { 67, 1, 0xC0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[2].steps[4]  = { 75, 1, 0xE0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[2].steps[6]  = { 70, 1, 0xC0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[2].steps[8]  = { 72, 1, 0xF0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[2].steps[10] = { 79, 1, 0xD0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[2].steps[12] = { 77, 1, 0xE0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };
    song.phrases[2].steps[14] = { 74, 1, 0xC0, {{CMD_NONE, 0}, {CMD_NONE, 0}, {CMD_NONE, 0}} };

    // ------------------------------------------------------------------------
    // CHAINS:
    // Chain 0: Bass (Phrase 0 x 4)
    // Chain 1: Drums (Phrase 1 x 4)
    // Chain 2: Arp (Phrase 2 x 4)
    // ------------------------------------------------------------------------
    for (int s = 0; s < 4; ++s) {
        song.chains[0].steps[s].phrase_id = 0;
        song.chains[0].steps[s].transpose = 0;

        song.chains[1].steps[s].phrase_id = 1;
        song.chains[1].steps[s].transpose = 0;

        song.chains[2].steps[s].phrase_id = 2;
        song.chains[2].steps[s].transpose = (s == 2) ? 3 : 0; // Transpose on 3rd measure
    }

    // ------------------------------------------------------------------------
    // SONG ROWS:
    // Row 0..3: Play Chains 0, 1, 2 on Tracks 0, 1, 2
    // ------------------------------------------------------------------------
    for (int r = 0; r < 4; ++r) {
        song.rows[r].chain_ids[0] = 0; // Track 0: Bass
        song.rows[r].chain_ids[1] = 1; // Track 1: Drums
        song.rows[r].chain_ids[2] = 2; // Track 2: Melody
        for (int t = 3; t < static_cast<int>(MAX_TRACKS); ++t) {
            song.rows[r].chain_ids[t] = CHAIN_EMPTY;
        }
    }

    return song;
}

} // namespace toad
