#include <catch2/catch_test_macros.hpp>
#include <toad/engine.h>
#include <vector>
#include <cmath>

TEST_CASE("Engine: Audio Rendering and Master Soft-Clipper", "[audio_render]") {
    toad::Engine engine;
    engine.setSampleRate(44100.0f);
    engine.setBpm(120.0f);

    toad::Song song;
    song.rows[0].chain_ids[0] = 0;
    song.chains[0].steps[0].phrase_id = 0;
    song.phrases[0].steps[0].note = 60; // Middle C
    song.phrases[0].steps[0].instrument = 0;
    song.phrases[0].steps[0].volume = 0xFF;

    // Instrument 0 is native Alpha Juno Synth
    song.instruments[0].type = toad::INST_TYPE_INTERNAL_SYNTH;
    song.instruments[0].synth.pulse_width = 0.5f;
    song.instruments[0].filter_type = toad::FLT_BYPASS;

    engine.loadSong(song);
    engine.play(toad::PLAY_SONG);

    SECTION("Deterministic block rendering generates audio") {
        float stereoBuffer[512 * 2]; // 512 stereo frames
        engine.renderBlockDeterministic(stereoBuffer, 512);

        float energy = 0.0f;
        for (int i = 0; i < 512 * 2; ++i) {
            energy += std::abs(stereoBuffer[i]);
            // Output must never exceed 0 dBFS (1.0f)
            REQUIRE(std::abs(stereoBuffer[i]) <= 1.0f);
        }
        REQUIRE(energy > 1.0f);
    }

    SECTION("Master output protection prevents clipping beyond 0 dBFS under extreme overdrive") {
        // Trigger all 8 tracks simultaneously with full volume and aggressive wavefolding
        for (size_t t = 0; t < toad::MAX_TRACKS; ++t) {
            song.rows[0].chain_ids[t] = 0;
            song.instruments[0].synth.fold_drive = 1.0f;
        }
        engine.loadSong(song);
        engine.play(toad::PLAY_SONG);

        float outLeft[256];
        float outRight[256];
        engine.renderBlockDeterministic(outLeft, outRight, 256);

        for (int i = 0; i < 256; ++i) {
            // Strict guardrail: branchless masterClip guarantees audio never exceeds 1.0f
            REQUIRE(outLeft[i] <= 1.0f);
            REQUIRE(outLeft[i] >= -1.0f);
            REQUIRE(outRight[i] <= 1.0f);
            REQUIRE(outRight[i] >= -1.0f);
        }
    }
}
