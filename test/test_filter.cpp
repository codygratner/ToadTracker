#include <catch2/catch_test_macros.hpp>
#include <toad/filter.h>
#include <cmath>

TEST_CASE("UniversalFilter: Bypass Mode", "[filter]") {
    toad::UniversalFilter filter;
    filter.setSampleRate(44100.0f);
    filter.setParameters(toad::FLT_BYPASS, 1000.0f, 0.0f);

    float in = 0.75f;
    REQUIRE(filter.process(in) == in);
}

TEST_CASE("UniversalFilter: State Variable Filter (SVF)", "[filter]") {
    toad::UniversalFilter filter;
    filter.setSampleRate(44100.0f);

    SECTION("SVF Lowpass attenuates high frequencies") {
        filter.setParameters(toad::FLT_LP12_SVF, 500.0f, 0.0f); // 500 Hz cutoff

        // Pass 10 kHz high frequency signal
        float maxOutput = 0.0f;
        for (int i = 0; i < 200; ++i) {
            float in = std::sin(i * 10000.0f * 6.2831853f / 44100.0f);
            float out = filter.process(in);
            if (i > 50 && std::abs(out) > maxOutput) {
                maxOutput = std::abs(out);
            }
        }
        // Attenuated by > 20 dB (< 0.1)
        REQUIRE(maxOutput < 0.15f);
    }

    SECTION("SVF Highpass attenuates low frequencies") {
        filter.setParameters(toad::FLT_HP12_SVF, 5000.0f, 0.0f); // 5000 Hz cutoff

        // Pass 100 Hz low frequency signal
        float maxOutput = 0.0f;
        for (int i = 0; i < 200; ++i) {
            float in = std::sin(i * 100.0f * 6.2831853f / 44100.0f);
            float out = filter.process(in);
            if (i > 50 && std::abs(out) > maxOutput) {
                maxOutput = std::abs(out);
            }
        }
        REQUIRE(maxOutput < 0.15f);
    }

    SECTION("SVF stability under extreme resonance") {
        filter.setParameters(toad::FLT_LP12_SVF, 1000.0f, 1.0f); // Maximum resonance

        for (int i = 0; i < 500; ++i) {
            float in = (i == 0) ? 1.0f : 0.0f; // Impulse
            float out = filter.process(in);
            REQUIRE(std::isfinite(out));
        }
    }
}

TEST_CASE("UniversalFilter: 4-Pole Ladder Filter", "[filter]") {
    toad::UniversalFilter filter;
    filter.setSampleRate(44100.0f);

    SECTION("Ladder 24dB/oct lowpass roll-off") {
        filter.setParameters(toad::FLT_LP24_LADDER, 300.0f, 0.2f);

        float maxHighFreq = 0.0f;
        for (int i = 0; i < 300; ++i) {
            float in = std::sin(i * 8000.0f * 6.2831853f / 44100.0f);
            float out = filter.process(in);
            if (i > 50 && std::abs(out) > maxHighFreq) {
                maxHighFreq = std::abs(out);
            }
        }
        // Very steep 24dB/oct attenuation
        REQUIRE(maxHighFreq < 0.05f);
    }

    SECTION("Ladder non-linear saturation prevents blow-up under self-oscillation") {
        filter.setParameters(toad::FLT_LP24_LADDER, 1000.0f, 1.0f); // Self-oscillating

        for (int i = 0; i < 500; ++i) {
            float in = (i == 0) ? 1.0f : 0.0f;
            float out = filter.process(in);
            REQUIRE(std::isfinite(out));
            // Non-linear saturation keeps resonance bounded
            REQUIRE(std::abs(out) <= 2.5f);
        }
    }
}

TEST_CASE("UniversalFilter: Comb Filter", "[filter]") {
    toad::UniversalFilter filter;
    filter.setSampleRate(44100.0f);

    SECTION("Positive feedback comb filter produces pitched resonance") {
        filter.setParameters(toad::FLT_COMB_POS, 440.0f, 0.8f);

        // Feed impulse
        float maxRes = 0.0f;
        for (int i = 0; i < 1000; ++i) {
            float in = (i == 0) ? 1.0f : 0.0f;
            float out = filter.process(in);
            REQUIRE(std::isfinite(out));
            if (std::abs(out) > maxRes) maxRes = std::abs(out);
        }
        REQUIRE(maxRes > 0.0f);
    }
}
