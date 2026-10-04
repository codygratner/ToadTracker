#include <catch2/catch_test_macros.hpp>
#include <toad/fast_math.h>
#include <toad/version.h>
#include <cmath>

TEST_CASE("FastMath: fastTanh and masterClip", "[fast_math]") {
    SECTION("fastTanh symmetry and origin") {
        REQUIRE(toad::FastMath::fastTanh(0.0f) == 0.0f);
        REQUIRE(toad::FastMath::fastTanh(-0.5f) == -toad::FastMath::fastTanh(0.5f));
        REQUIRE(toad::FastMath::fastTanh(-1.0f) == -toad::FastMath::fastTanh(1.0f));
    }

    SECTION("fastTanh rational approximation error < 2% within [-1.0, 1.0]") {
        for (float x = -1.0f; x <= 1.0f; x += 0.05f) {
            float approx = toad::FastMath::fastTanh(x);
            float exact = std::tanh(x);
            float diff = std::abs(approx - exact);
            REQUIRE(diff <= 0.025f);
        }
    }

    SECTION("masterClip hard saturation limits") {
        REQUIRE(toad::FastMath::masterClip(1.51f) == 1.0f);
        REQUIRE(toad::FastMath::masterClip(10.0f) == 1.0f);
        REQUIRE(toad::FastMath::masterClip(-1.51f) == -1.0f);
        REQUIRE(toad::FastMath::masterClip(-100.0f) == -1.0f);

        // Within [-1.0, 1.0], masterClip matches fastTanh
        REQUIRE(toad::FastMath::masterClip(0.5f) == toad::FastMath::fastTanh(0.5f));
        REQUIRE(toad::FastMath::masterClip(-0.5f) == toad::FastMath::fastTanh(-0.5f));
    }
}

TEST_CASE("FastMath: Phase Accumulator Step Calculation", "[fast_math]") {
    SECTION("Zero frequency produces zero phase increment") {
        REQUIRE(toad::FastMath::calculatePhaseIncrement(0.0f, 44100.0f) == 0);
    }

    SECTION("Nyquist frequency produces 0x80000000 (half of 32-bit uint)") {
        uint32_t nyquistInc = toad::FastMath::calculatePhaseIncrement(22050.0f, 44100.0f);
        REQUIRE(nyquistInc == 2147483648u); // 0x80000000
    }

    SECTION("Standard 440 Hz phase step at 44100 Hz") {
        uint32_t inc = toad::FastMath::calculatePhaseIncrement(440.0f, 44100.0f);
        // (440.0 / 44100.0) * 4294967296.0 = 42852281
        REQUIRE(inc >= 42852280u);
        REQUIRE(inc <= 42852282u);
    }

    SECTION("Invalid sample rate does not divide by zero") {
        REQUIRE(toad::FastMath::calculatePhaseIncrement(440.0f, 0.0f) == 0);
        REQUIRE(toad::FastMath::calculatePhaseIncrement(440.0f, -44100.0f) == 0);
    }
}

TEST_CASE("FastMath: Pitch and Exponential Calculation Accuracy", "[fast_math]") {
    SECTION("fast2Exp approximation matches powers of 2 within 3.5%") {
        // Test integer powers of 2
        for (float p = -4.0f; p <= 4.0f; p += 1.0f) {
            float approx = toad::FastMath::fast2Exp(p);
            float exact = std::pow(2.0f, p);
            float relative_error = std::abs(approx - exact) / exact;
            REQUIRE(relative_error < 0.035f);
        }
    }

    SECTION("semitoneToRatio octave scaling") {
        float root = toad::FastMath::semitoneToRatio(0.0f);
        float octaveUp = toad::FastMath::semitoneToRatio(12.0f);
        float octaveDown = toad::FastMath::semitoneToRatio(-12.0f);

        REQUIRE(std::abs(root - 1.0f) < 0.04f);
        REQUIRE(std::abs(octaveUp - 2.0f) < 0.08f);
        REQUIRE(std::abs(octaveDown - 0.5f) < 0.02f);

        // Perfect fifth (7 semitones = 2^(7/12) ~ 1.4983)
        float fifth = toad::FastMath::semitoneToRatio(7.0f);
        float exactFifth = std::pow(2.0f, 7.0f / 12.0f);
        REQUIRE(std::abs(fifth - exactFifth) / exactFifth < 0.035f);
    }

    SECTION("noteToFrequency MIDI tuning (A4 = 440 Hz)") {
        float a4 = toad::FastMath::noteToFrequency(69.0f);
        float a5 = toad::FastMath::noteToFrequency(81.0f);
        float a3 = toad::FastMath::noteToFrequency(57.0f);
        float c4 = toad::FastMath::noteToFrequency(60.0f); // Middle C = 261.63 Hz

        REQUIRE(std::abs(a4 - 440.0f) / 440.0f < 0.035f);
        REQUIRE(std::abs(a5 - 880.0f) / 880.0f < 0.035f);
        REQUIRE(std::abs(a3 - 220.0f) / 220.0f < 0.035f);
        REQUIRE(std::abs(c4 - 261.63f) / 261.63f < 0.035f);
    }
}

TEST_CASE("FastMath: Parabolic fastSin", "[fast_math]") {
    SECTION("Key cycle points") {
        REQUIRE(toad::FastMath::fastSin(0.0f) == 0.0f);
        REQUIRE(toad::FastMath::fastSin(0.5f) == 1.0f);
        REQUIRE(toad::FastMath::fastSin(-0.5f) == -1.0f);
        REQUIRE(toad::FastMath::fastSin(1.0f) == 0.0f);
        REQUIRE(toad::FastMath::fastSin(-1.0f) == 0.0f);
    }
}

TEST_CASE("FastMath: Deterministic Sequencer Timing Subdivision", "[fast_math]") {
    SECTION("44.1 kHz at 120 BPM") {
        // SamplesPerTick = (44100 * 60) / (120 * 24 * 1.0) = 2646000 / 2880 = 918.75
        float samplesPerTick = toad::FastMath::calculateSamplesPerTick(44100.0f, 120.0f);
        REQUIRE(samplesPerTick == 918.75f);

        // 6 ticks per step = 918.75 * 6 = 5512.5 samples per step
        float samplesPerStep = toad::FastMath::calculateSamplesPerStep(44100.0f, 120.0f, 6);
        REQUIRE(samplesPerStep == 5512.5f);
    }

    SECTION("48.0 kHz at 120 BPM") {
        // SamplesPerTick = (48000 * 60) / (120 * 24 * 1.0) = 2880000 / 2880 = 1000.0
        float samplesPerTick = toad::FastMath::calculateSamplesPerTick(48000.0f, 120.0f);
        REQUIRE(samplesPerTick == 1000.0f);

        // 6 ticks per step = 6000.0 samples
        float samplesPerStep = toad::FastMath::calculateSamplesPerStep(48000.0f, 120.0f, 6);
        REQUIRE(samplesPerStep == 6000.0f);
    }

    SECTION("Groove multiplier scaling") {
        // Double groove multiplier halves tick duration
        float halfTick = toad::FastMath::calculateSamplesPerTick(48000.0f, 120.0f, 2.0f);
        REQUIRE(halfTick == 500.0f);
    }
}

TEST_CASE("FastMath: Architecture Invariants (ARM & x86_64 Compatibility)", "[fast_math]") {
    SECTION("Host platform is little-endian") {
        REQUIRE(std::endian::native == std::endian::little);
    }

    SECTION("IEEE-754 32-bit floating point compliance") {
        REQUIRE(sizeof(float) == 4);
        REQUIRE(sizeof(uint32_t) == 4);
        REQUIRE(std::numeric_limits<float>::is_iec559);
    }

    SECTION("bit_cast roundtrip preserves binary representation") {
        uint32_t original_pattern = 0x3F800000; // 1.0f in IEEE 754
        float as_float = std::bit_cast<float>(original_pattern);
        REQUIRE(as_float == 1.0f);
        uint32_t roundtrip = std::bit_cast<uint32_t>(as_float);
        REQUIRE(roundtrip == original_pattern);
    }
}

TEST_CASE("Version: Metadata and Numeric Invariants", "[version]") {
    SECTION("Version 0.1.1 string and components") {
        REQUIRE(toad::VERSION_MAJOR == 0);
        REQUIRE(toad::VERSION_MINOR == 1);
        REQUIRE(toad::VERSION_PATCH == 1);
        REQUIRE(toad::VERSION_STRING == "0.1.1");
        REQUIRE(toad::getVersionString() == "0.1.1");
        REQUIRE(toad::getVersionNumber() == 0x000101);
        REQUIRE(toad::APP_NAME == "ToadTracker");
        REQUIRE(toad::CODENAME == "The Toad");
    }
}


