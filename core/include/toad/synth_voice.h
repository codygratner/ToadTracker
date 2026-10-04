#pragma once

#include "types.h"
#include "fast_math.h"
#include "filter.h"
#include <cmath>
#include <array>
#include <algorithm>

namespace toad {

// PolyBLEP anti-aliasing residual correction for bandlimited wave synthesis
inline float polyBLEP(float phase, float phaseInc) {
    if (phase < phaseInc) {
        float t = phase / phaseInc;
        return t + t - t * t - 1.0f;
    }
    if (phase > 1.0f - phaseInc) {
        float t = (phase - 1.0f) / phaseInc;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

class SynthVoice {
public:
    SynthVoice() = default;

    void setSampleRate(float sampleRateHz) {
        if (sampleRateHz > 0.0f) {
            sampleRate_ = sampleRateHz;
            filter_.setSampleRate(sampleRateHz);
            reset();
        }
    }

    void reset() {
        phase_ = 0.0f;
        phaseInc_ = 0.0f;
        note_ = NOTE_EMPTY;
        active_ = false;
        envLevel_ = 0.0f;
        filter_.reset();
        for (auto& st : allpassBuffer_) st = 0.0f;
        for (auto& st : allpassX1_) st = 0.0f;
    }

    void noteOn(uint8_t note, uint8_t velocity, const SynthConfig& config, const Instrument& inst) {
        note_ = note;
        velocity_ = velocity;
        active_ = true;

        // Calculate pitch frequency
        frequencyHz_ = FastMath::noteToFrequency(static_cast<float>(note));
        phaseInc_ = frequencyHz_ / sampleRate_;
        if (phaseInc_ > 0.49f) phaseInc_ = 0.49f;

        // Configure synth parameters
        pulseWidth_ = std::clamp(config.pulse_width, 0.01f, 0.99f);
        foldDrive_ = std::max(0.0f, config.fold_drive);
        disperserFreq_ = std::clamp(config.disperser_freq, 20.0f, sampleRate_ * 0.45f);
        disperserStages_ = std::clamp<uint8_t>(config.disperser_stages, 1, 8);

        // Simple fast anti-click attack
        envLevel_ = static_cast<float>(velocity) / 255.0f;

        // Configure filter
        filter_.setParameters(inst.filter_type, inst.filter_cutoff, inst.filter_resonance, inst.drive_amount);
    }

    void noteOff() {
        active_ = false;
        envLevel_ = 0.0f;
    }

    bool isActive() const { return active_; }
    uint8_t getNote() const { return note_; }

    void setPulseWidth(float pw) {
        pulseWidth_ = std::clamp(pw, 0.01f, 0.99f);
    }

    void setFoldDrive(float drive) {
        foldDrive_ = std::max(0.0f, drive);
    }

    void setDisperser(float freqHz, uint8_t stages = 1) {
        disperserFreq_ = std::clamp(freqHz, 20.0f, sampleRate_ * 0.45f);
        disperserStages_ = std::clamp<uint8_t>(stages, 1, 8);
    }

    void setFilterParameters(FilterType type, float cutoffHz, float resonance, float drive = 0.0f) {
        filter_.setParameters(type, cutoffHz, resonance, drive);
    }

    void setPitch(float note) {
        frequencyHz_ = FastMath::noteToFrequency(note);
        phaseInc_ = frequencyHz_ / sampleRate_;
        if (phaseInc_ > 0.49f) phaseInc_ = 0.49f;
    }

    // Process a single audio sample through the Alpha Juno, Wavefolder, Disperser, and Filter
    inline float process() {
        if (!active_ || phaseInc_ <= 0.0f) {
            return 0.0f;
        }

        // --- 1. ALPHA JUNO STYLE VARIABLE-SLOPE SAW/PULSE ---
        float rawWave = 0.0f;
        float pw = pulseWidth_;

        if (phase_ < pw) {
            rawWave = -1.0f + 2.0f * (phase_ / pw);
        } else {
            rawWave = 1.0f - 2.0f * ((phase_ - pw) / (1.0f - pw));
        }

        // Apply PolyBLEP to both transitions for anti-aliased sharpness
        rawWave += polyBLEP(phase_, phaseInc_);
        float midPhase = phase_ - pw;
        if (midPhase < 0.0f) midPhase += 1.0f;
        rawWave -= polyBLEP(midPhase, phaseInc_);

        // Advance phase accumulator
        phase_ += phaseInc_;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
        }

        // --- 2. NON-LINEAR WAVEFOLDER (CMD_FOLD) ---
        float folded = rawWave;
        if (foldDrive_ > 0.0f) {
            float driveScale = 1.0f + foldDrive_ * 5.0f;
            float driven = rawWave * driveScale;

            // Non-linear foldback reflection
            float x = driven;
            while (x > 1.0f || x < -1.0f) {
                if (x > 1.0f)  x = 2.0f - x;
                if (x < -1.0f) x = -2.0f - x;
            }
            // Soft-saturate folded output
            folded = FastMath::fastTanh(x * 1.2f);
        }

        // --- 3. ALLPASS DISPERSER (CMD_DISP) ---
        float dispersed = folded;
        if (disperserFreq_ > 0.0f && disperserStages_ > 0) {
            // First-order allpass coefficient
            float w = 3.14159265f * disperserFreq_ / sampleRate_;
            float tanW = (w < 0.4f) ? w * (1.0f + (w * w) / 3.0f) : std::tan(w);
            float c = (tanW - 1.0f) / (tanW + 1.0f);

            // Cascade through 1..8 allpass delay stages
            for (uint8_t i = 0; i < disperserStages_; ++i) {
                float y = c * (dispersed - allpassBuffer_[i]) + allpassX1_[i];
                allpassX1_[i] = dispersed;
                allpassBuffer_[i] = y;
                dispersed = y;
            }
        }

        // --- 4. UNIVERSAL FILTER STAGE ---
        float filtered = filter_.process(dispersed);

        // Scale by velocity/envelope
        return filtered * envLevel_;
    }

private:
    float sampleRate_{44100.0f};
    float frequencyHz_{440.0f};
    float phase_{0.0f};
    float phaseInc_{0.0f};
    float pulseWidth_{0.5f};
    float foldDrive_{0.0f};
    float disperserFreq_{1000.0f};
    uint8_t disperserStages_{1};

    uint8_t note_{NOTE_EMPTY};
    uint8_t velocity_{0};
    bool active_{false};
    float envLevel_{0.0f};

    UniversalFilter filter_;

    // Disperser state (up to 8 stages)
    std::array<float, 8> allpassBuffer_{};
    std::array<float, 8> allpassX1_{};
};

} // namespace toad
