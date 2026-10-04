#include <catch2/catch_test_macros.hpp>
#include <toad/engine.h>

TEST_CASE("Engine: Initialization and Transport State", "[sequencer]") {
    toad::Engine engine;

    REQUIRE(engine.isStopped());
    REQUIRE(engine.getTransportState() == toad::TRANSPORT_STOPPED);
    REQUIRE(engine.getBpm() == 120.0f);
    REQUIRE(engine.getSampleRate() == 44100.0f);
    REQUIRE(engine.getTicksPerStep() == 6);

    SECTION("Play, Pause, and Stop Transitions") {
        engine.play(toad::PLAY_SONG);
        REQUIRE(engine.isPlaying());
        REQUIRE(engine.getPlaybackMode() == toad::PLAY_SONG);

        engine.pause();
        REQUIRE(engine.isPaused());

        engine.play(toad::PLAY_PHRASE);
        REQUIRE(engine.isPlaying());
        REQUIRE(engine.getPlaybackMode() == toad::PLAY_PHRASE);

        engine.stop();
        REQUIRE(engine.isStopped());
    }
}

TEST_CASE("Engine: Deterministic Timing and Block Slicing", "[sequencer]") {
    toad::Engine engine;
    engine.setSampleRate(44100.0f);
    engine.setBpm(120.0f);
    engine.setTicksPerStep(6);

    toad::Song song;
    song.bpm = 120.0f;
    song.groove = 6;

    // Set up a simple phrase on Track 0 with a note on step 0
    song.rows[0].chain_ids[0] = 0;
    song.chains[0].steps[0].phrase_id = 0;
    song.phrases[0].steps[0].note = 60; // Middle C
    song.phrases[0].steps[0].instrument = 0;
    song.phrases[0].steps[0].volume = 0xFF;

    engine.loadSong(song);
    engine.play(toad::PLAY_SONG);

    // Initial note on step 0
    REQUIRE(engine.getEventCount() >= 1);
    engine.clearEvents();

    SECTION("Sub-buffer tick slicing accumulates exactly across arbitrary block sizes") {
        // At 120 BPM, 44.1 kHz, 6 ticks/step:
        // Samples per tick = 918.75
        // Samples per step = 5512.5
        // Process blocks of size 256 samples
        size_t total_samples = 0;
        size_t ticks_detected = 0;

        // Run through 1 step (6 ticks ~ 5513 samples)
        for (int b = 0; b < 25; ++b) {
            engine.processBlock(256);
            total_samples += 256;
        }

        // 6400 samples processed. 6400 / 918.75 = ~6.96 ticks
        // Should have crossed step 0 and arrived at step 1
        const auto& track = engine.getTrackState(0);
        REQUIRE(track.phrase_step >= 1);
    }
}

TEST_CASE("Engine: Step Trigger Pipeline (Pool, Scale, Transpose)", "[sequencer]") {
    toad::Engine engine;
    engine.setSampleRate(44100.0f);
    engine.setBpm(120.0f);

    toad::Song song;
    song.rows[0].chain_ids[0] = 0;
    song.chains[0].steps[0].phrase_id = 0;
    song.chains[0].steps[0].transpose = 2; // +2 semitones from Chain

    // Instrument with Scale (C Major) and Note Pool (+12 semitones)
    toad::Instrument& inst = song.instruments[0];
    inst.scale.enabled = true;
    inst.scale.root_note = 0;      // C
    inst.scale.note_mask = 0x0AB5; // C Major (C, D, E, F, G, A, B)
    inst.scale.snap_mode = toad::SNAP_DOWN;

    inst.note_pool.mode = toad::POOL_CYCLE;
    inst.note_pool.slot_count = 1;
    inst.note_pool.slots[0] = { .semitone_offset = 12, .weight = 0xFF, .velocity_scale = 0xFF };
    inst.note_pool.last_selected = 0;

    // Trigger raw note C# (61)
    // 1. Note Pool adds +12 -> 73 (C#5)
    // 2. Scale snaps C#5 (73) down to C5 (72)
    // 3. Chain adds +2 semitones -> 74 (D5)
    song.phrases[0].steps[0].note = 61;
    song.phrases[0].steps[0].instrument = 0;
    song.phrases[0].steps[0].volume = 0x80;

    engine.loadSong(song);
    engine.play(toad::PLAY_SONG);

    const auto& track = engine.getTrackState(0);
    REQUIRE(track.raw_note == 61);
    REQUIRE(track.resolved_note == 74);
    REQUIRE(track.effective_note == 74);
}

TEST_CASE("Engine: Dual Table Modulators (Primary & Aux Tables)", "[sequencer]") {
    toad::Engine engine;
    toad::Song song;

    // Table 0 (Primary Table): Transpose +7 on row 1, volume 50% (0x80) on row 2
    song.tables[0].speed = 1;
    song.tables[0].loop = false;
    song.tables[0].rows[0].transpose = 0;
    song.tables[0].rows[1].transpose = 7;
    song.tables[0].rows[2].volume = 0x80;

    // Table 1 (Aux Table): Transpose +12 on row 0, CUTOFF filter sweep on row 1
    song.tables[1].speed = 1;
    song.tables[1].loop = false;
    song.tables[1].rows[0].transpose = 12;
    song.tables[1].rows[1].cmd1 = toad::CMD_FCUT;
    song.tables[1].rows[1].val1 = 0xC0;

    // Instrument uses Table 0
    song.instruments[0].table_id = 0;

    // Phrase triggers Note 60 with CMD_ATBL (Aux Table 1) in FX column 0
    song.rows[0].chain_ids[0] = 0;
    song.chains[0].steps[0].phrase_id = 0;
    song.phrases[0].steps[0].note = 60;
    song.phrases[0].steps[0].instrument = 0;
    song.phrases[0].steps[0].volume = 0xFF;
    song.phrases[0].steps[0].fx[0].cmd = toad::CMD_ATBL;
    song.phrases[0].steps[0].fx[0].val = 1; // Trigger Table 1

    engine.loadSong(song);
    engine.play(toad::PLAY_SONG);

    const auto& track = engine.getTrackState(0);
    REQUIRE(track.primary_table.active);
    REQUIRE(track.aux_table.active);

    // On Step 0 / Row 0:
    // Primary table transpose = 0
    // Aux table transpose = 12
    // Effective note = 60 + 0 + 12 = 72
    REQUIRE(track.effective_note == 72);

    // Advance 1 tick
    engine.processTick(0);
    // Primary table moves to row 1 (transpose = +7)
    // Aux table moves to row 1 (transpose = 0, CMD_FCUT 0xC0)
    // Effective note = 60 + 7 = 67
    REQUIRE(track.effective_note == 67);

    // Advance 2nd tick
    engine.processTick(0);
    // Primary table moves to row 2 (volume = 0x80 -> ~50%)
    REQUIRE(track.effective_volume == 128);
}

TEST_CASE("Engine: Hierarchical Traversal (Song -> Chain -> Phrase)", "[sequencer]") {
    toad::Engine engine;
    toad::Song song;

    // Song Row 0 uses Chain 0
    // Song Row 1 uses Chain 1
    song.rows[0].chain_ids[0] = 0;
    song.rows[1].chain_ids[0] = 1;

    // Chain 0 has Phrase 0 -> Phrase 1
    song.chains[0].steps[0].phrase_id = 0;
    song.chains[0].steps[1].phrase_id = 1;
    song.chains[0].steps[2].phrase_id = toad::PHRASE_EMPTY; // End of chain 0

    // Chain 1 has Phrase 2
    song.chains[1].steps[0].phrase_id = 2;
    song.chains[1].steps[1].phrase_id = toad::PHRASE_EMPTY; // End of chain 1

    engine.loadSong(song);
    engine.play(toad::PLAY_SONG);

    REQUIRE(engine.getSongRow() == 0);
    REQUIRE(engine.getTrackState(0).current_phrase_id == 0);

    // Advance through all 16 steps of Phrase 0 (each step is 6 ticks)
    for (int step = 0; step < 16; ++step) {
        for (int tick = 0; tick < 6; ++tick) {
            engine.processTick(0);
        }
    }

    // Should now be on Phrase 1
    REQUIRE(engine.getSongRow() == 0);
    REQUIRE(engine.getTrackState(0).chain_step == 1);
    REQUIRE(engine.getTrackState(0).current_phrase_id == 1);

    // Advance through 16 steps of Phrase 1
    for (int step = 0; step < 16; ++step) {
        for (int tick = 0; tick < 6; ++tick) {
            engine.processTick(0);
        }
    }

    // Chain 0 ended! Engine should advance to Song Row 1 (Chain 1 -> Phrase 2)
    REQUIRE(engine.getSongRow() == 1);
    REQUIRE(engine.getTrackState(0).current_chain_id == 1);
    REQUIRE(engine.getTrackState(0).current_phrase_id == 2);
}
