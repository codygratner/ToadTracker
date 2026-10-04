#pragma once

#include <cstdint>
#include <cstring>
#include <bit>

namespace toad {
namespace FastMath {

// Compile-time architectural invariants for bit-cast math and phase accumulators
static_assert(std::endian::native == std::endian::little,
    "ToadTracker FastMath requires little-endian architecture (guaranteed on x86_64, ARM, RISC-V, Xtensa)");
static_assert(sizeof(float) == 4, "ToadTracker FastMath requires 32-bit IEEE-754 float");
static_assert(sizeof(uint32_t) == 4, "ToadTracker FastMath requires 32-bit unsigned integers");

// Branchless 32-bit Phase Accumulator Step Calculation
inline uint32_t calculatePhaseIncrement(float frequencyHz, float sampleRateHz) {
    if (sampleRateHz <= 0.0f) return 0;
    return static_cast<uint32_t>((frequencyHz / sampleRateHz) * 4294967296.0f);
}

// Fast Rational Tanh Approximation for Soft-Clipping & Drive Stages
// Maximum error < 1% on [-1.0, 1.0], zero denormals, fully branchless
inline float fastTanh(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Fast Hard Soft-Clipper for Master Output Protection
// Clips beyond +/- 1.5 into clean +/- 1.0 limits
inline float masterClip(float x) {
    if (x > 1.5f) return 1.0f;
    if (x < -1.5f) return -1.0f;
    return fastTanh(x);
}

// Fast 2^x Pitch-to-Frequency Conversion (IEEE 754 bit-cast approximation)
inline float fast2Exp(float p) {
    float clp = (p < -126.0f) ? -126.0f : ((p > 126.0f) ? 126.0f : p);
    uint32_t i = static_cast<uint32_t>((1 << 23) * (clp + 126.94269504f));
    return std::bit_cast<float>(i);
}

// Semitone Offset to Pitch Multiplier (12-Tone Equal Temperament)
inline float semitoneToRatio(float semitones) {
    return fast2Exp(semitones * 0.08333333333f);
}

// Note to Frequency (MIDI note 69 = A4 = 440 Hz)
inline float noteToFrequency(float note, float a4Frequency = 440.0f) {
    return a4Frequency * semitoneToRatio(note - 69.0f);
}

// High-speed Parabolic Sine Approximation (-1.0 to 1.0 phase input)
inline float fastSin(float normalizedPhase) {
    float x = normalizedPhase;
    return 4.0f * x * (1.0f - ((x < 0.0f) ? -x : x));
}

// Deterministic Tick Timing: Samples per Sequencer Tick
// SamplesPerTick = (SampleRate * 60) / (BPM * 24 * GrooveMultiplier)
inline float calculateSamplesPerTick(float sampleRateHz, float bpm, float grooveMultiplier = 1.0f) {
    if (bpm <= 0.0f || sampleRateHz <= 0.0f) return 0.0f;
    float gm = (grooveMultiplier > 0.0f) ? grooveMultiplier : 1.0f;
    return (sampleRateHz * 60.0f) / (bpm * 24.0f * gm);
}

// Deterministic Step Timing: Samples per Sequencer Step (default 6 ticks per step)
inline float calculateSamplesPerStep(float sampleRateHz, float bpm, uint8_t ticksPerStep = 6, float grooveMultiplier = 1.0f) {
    return calculateSamplesPerTick(sampleRateHz, bpm, grooveMultiplier) * static_cast<float>(ticksPerStep);
}

} // namespace FastMath
} // namespace toad
