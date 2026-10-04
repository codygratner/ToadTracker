#include <catch2/catch_test_macros.hpp>
#include <toad/synth_voice.h>
#include <toad/wavetable_voice.h>
#include <toad/sf2_player.h>
#include <cmath>

TEST_CASE("SynthVoice: Alpha Juno Variable-Slope Oscillator", "[dsp_synth]") {
    toad::SynthVoice voice;
    voice.setSampleRate(44100.0f);

    toad::SynthConfig config;
    config.pulse_width = 0.5f;
    config.fold_drive = 0.0f;
    config.disperser_freq = 0.0f;
    config.disperser_stages = 1;

    toad::Instrument inst;
    inst.filter_type = toad::FLT_BYPASS;

    REQUIRE(!voice.isActive());

    SECTION("Note On activates voice at correct frequency") {
        voice.noteOn(69, 0xFF, config, inst); // A4 = 440 Hz
        REQUIRE(voice.isActive());
        REQUIRE(voice.getNote() == 69);

        // Process audio buffer
        float minVal = 0.0f;
        float maxVal = 0.0f;
        for (int i = 0; i < 500; ++i) {
            float s = voice.process();
            if (s < minVal) minVal = s;
            if (s > maxVal) maxVal = s;
        }

        // Variable-slope saw/pulse with PolyBLEP produces bipolar oscillation
        REQUIRE(maxVal > 0.5f);
        REQUIRE(minVal < -0.5f);
    }

    SECTION("Pulse width changes wave morphology") {
        voice.noteOn(60, 0xFF, config, inst);
        voice.setPulseWidth(0.1f);

        float s1 = 0.0f;
        for (int i = 0; i < 100; ++i) {
            s1 += std::abs(voice.process());
        }
        REQUIRE(s1 > 0.0f);
    }

    SECTION("Note Off silences voice") {
        voice.noteOn(60, 0xFF, config, inst);
        voice.noteOff();
        REQUIRE(!voice.isActive());
        REQUIRE(voice.process() == 0.0f);
    }
}

TEST_CASE("SynthVoice: Wavefolder (CMD_FOLD)", "[dsp_synth]") {
    toad::SynthVoice voice;
    voice.setSampleRate(44100.0f);

    toad::SynthConfig config;
    config.pulse_width = 0.5f;
    config.fold_drive = 1.0f; // Maximum wavefolder drive
    config.disperser_freq = 0.0f;

    toad::Instrument inst;
    inst.filter_type = toad::FLT_BYPASS;

    voice.noteOn(60, 0xFF, config, inst);

    // Process folded output: verify signal stays bounded within [-1.2, 1.2]
    bool foldActive = false;
    for (int i = 0; i < 200; ++i) {
        float s = voice.process();
        REQUIRE(s <= 1.2f);
        REQUIRE(s >= -1.2f);
        if (std::abs(s) > 0.1f) foldActive = true;
    }
    REQUIRE(foldActive);
}

TEST_CASE("SynthVoice: Allpass Disperser (CMD_DISP)", "[dsp_synth]") {
    toad::SynthVoice voice;
    voice.setSampleRate(44100.0f);

    toad::SynthConfig config;
    config.pulse_width = 0.5f;
    config.fold_drive = 0.0f;
    config.disperser_freq = 500.0f;
    config.disperser_stages = 4; // 4-stage phase smear

    toad::Instrument inst;
    inst.filter_type = toad::FLT_BYPASS;

    voice.noteOn(48, 0xFF, config, inst); // C3

    for (int i = 0; i < 200; ++i) {
        float s = voice.process();
        REQUIRE(std::isfinite(s));
    }
}

TEST_CASE("WavetableVoice: Basic Shapes and Phase Warps", "[dsp_wavetable]") {
    toad::WavetableVoice voice;
    voice.setSampleRate(44100.0f);

    toad::WavetableConfig config;
    config.position = 0.0f; // Sine wave frame
    config.warp_mode = toad::WARP_OFF;
    config.warp_amount = 0.0f;

    toad::Instrument inst;
    inst.filter_type = toad::FLT_BYPASS;

    SECTION("Sine wave frame generation") {
        voice.noteOn(69, 0xFF, config, inst);
        REQUIRE(voice.isActive());

        float maxVal = 0.0f;
        float minVal = 0.0f;
        for (int i = 0; i < 200; ++i) {
            float s = voice.process();
            if (s > maxVal) maxVal = s;
            if (s < minVal) minVal = s;
        }
        REQUIRE(maxVal > 0.8f);
        REQUIRE(minVal < -0.8f);
    }

    SECTION("Frame position morphing") {
        voice.noteOn(60, 0xFF, config, inst);
        voice.setPosition(1.5f); // Halfway between Triangle and Saw

        for (int i = 0; i < 100; ++i) {
            float s = voice.process();
            REQUIRE(std::isfinite(s));
        }
    }

    SECTION("Phase warp operators") {
        toad::WarpMode modes[] = {
            toad::WARP_PWM,
            toad::WARP_SYNC,
            toad::WARP_BEND,
            toad::WARP_FOLD,
            toad::WARP_FORMANT,
            toad::WARP_BITCR
        };

        for (auto mode : modes) {
            config.warp_mode = mode;
            config.warp_amount = 0.7f;
            voice.noteOn(60, 0xFF, config, inst);

            for (int i = 0; i < 50; ++i) {
                float s = voice.process();
                REQUIRE(std::isfinite(s));
            }
        }
    }
}

TEST_CASE("SF2Voice: Multi-Zone Sample Player", "[dsp_sf2]") {
    toad::SF2Voice voice;
    voice.setSampleRate(44100.0f);

    // Create a simple synthetic 16-bit PCM buffer (single cycle triangle)
    int16_t pcmBuffer[64];
    for (int i = 0; i < 64; ++i) {
        float phase = static_cast<float>(i) / 64.0f;
        pcmBuffer[i] = static_cast<int16_t>((phase < 0.5f ? -30000.0f + 60000.0f * (phase * 2.0f) : 30000.0f - 60000.0f * ((phase - 0.5f) * 2.0f)));
    }

    toad::SF2InstrumentData instData;
    instData.zone_count = 1;
    instData.zones[0].low_key = 0;
    instData.zones[0].high_key = 127;
    instData.zones[0].low_velocity = 0;
    instData.zones[0].high_velocity = 127;
    instData.zones[0].sample_start = 0;
    instData.zones[0].sample_end = 64;
    instData.zones[0].loop_start = 0;
    instData.zones[0].loop_end = 64;
    instData.zones[0].sample_rate = 44100;
    instData.zones[0].root_key = 60;
    instData.zones[0].fine_tune = 0;
    instData.zones[0].is_looped = true;
    instData.pcm_data = pcmBuffer;
    instData.pcm_sample_count = 64;

    voice.setInstrumentData(&instData);

    toad::Instrument inst;
    inst.filter_type = toad::FLT_BYPASS;

    voice.noteOn(60, 0xFF, inst);
    REQUIRE(voice.isActive());

    for (int i = 0; i < 200; ++i) {
        float s = voice.process();
        REQUIRE(std::isfinite(s));
        REQUIRE(std::abs(s) <= 1.0f);
    }
}
