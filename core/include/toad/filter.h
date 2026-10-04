#pragma once

#include "types.h"
#include "fast_math.h"
#include <cmath>
#include <array>
#include <algorithm>

namespace toad {

// Maximum delay line size for comb filters (~20 Hz at 96 kHz)
constexpr size_t COMB_MAX_DELAY = 4800;

class UniversalFilter {
public:
    UniversalFilter() = default;

    void setSampleRate(float sampleRateHz) {
        if (sampleRateHz > 0.0f) {
            sampleRate_ = sampleRateHz;
            combWritePos_ = 0;
            combBuffer_.fill(0.0f);
            reset();
        }
    }

    void reset() {
        // Reset SVF integrators
        s1_ = 0.0f;
        s2_ = 0.0f;
        // Reset Ladder stages
        ladderStage_.fill(0.0f);
        // Reset Comb delay buffer
        combBuffer_.fill(0.0f);
        combWritePos_ = 0;
    }

    void setParameters(FilterType type, float cutoffHz, float resonance, float drive = 0.0f) {
        type_ = type;
        cutoffHz_ = std::clamp(cutoffHz, 20.0f, sampleRate_ * 0.49f);
        resonance_ = std::clamp(resonance, 0.0f, 1.0f);
        drive_ = std::max(0.0f, drive);
    }

    // Process a single audio sample (zero dynamic allocation, branchless core where possible)
    inline float process(float input) {
        if (type_ == FLT_BYPASS) {
            return input;
        }

        // Apply pre-filter drive if configured
        float x = input;
        if (drive_ > 0.0f) {
            x = FastMath::fastTanh(x * (1.0f + drive_));
        }

        switch (type_) {
            case FLT_LP12_SVF:
            case FLT_HP12_SVF:
            case FLT_BP12_SVF:
            case FLT_NOTCH_SVF:
            case FLT_PEAK_SVF:
                return processSVF(x);

            case FLT_LP24_LADDER:
                return processLadder(x);

            case FLT_COMB_POS:
                return processComb(x, 1.0f);

            case FLT_COMB_NEG:
                return processComb(x, -1.0f);

            default:
                return x;
        }
    }

private:
    // State Variable Filter (Topology-Preserving Transform / Zero-Delay Feedback)
    inline float processSVF(float input) {
        // Prewarp cutoff frequency into normalized digital domain
        float w = 3.14159265f * cutoffHz_ / sampleRate_;
        // Fast tangent approximation for small w, standard tan for precision
        float g = (w < 0.4f) ? w * (1.0f + (w * w) / 3.0f) : std::tan(w);

        // Q from 0.5 (Butterworth damping) to 25.0 (high resonance)
        float q = 0.5f + resonance_ * 24.5f;
        float k = 1.0f / q;

        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;

        float v3 = input - s2_;
        float v1 = a1 * s1_ + a2 * v3;
        float v2 = s2_ + a2 * s1_ + a3 * v3;

        s1_ = 2.0f * v1 - s1_;
        s2_ = 2.0f * v2 - s2_;

        // Flush subnormal float values
        if (std::abs(s1_) < 1.0e-15f) s1_ = 0.0f;
        if (std::abs(s2_) < 1.0e-15f) s2_ = 0.0f;

        switch (type_) {
            case FLT_LP12_SVF:  return v2;
            case FLT_HP12_SVF:  return input - k * v1 - v2;
            case FLT_BP12_SVF:  return v1;
            case FLT_NOTCH_SVF: return input - k * v1;
            case FLT_PEAK_SVF:  return 2.0f * v2 - input + k * v1;
            default:            return v2;
        }
    }

    // 4-Pole Moog-Style Transistor Ladder Filter with non-linear saturation
    inline float processLadder(float input) {
        // Normalized frequency coefficient
        float wc = 2.0f * 3.14159265f * cutoffHz_ / sampleRate_;
        float g = 1.0f - std::exp(-wc);

        // Resonance feedback coefficient (0.0 to 3.99 for self-oscillation control)
        float resCoeff = resonance_ * 3.95f;

        // Feedback with soft clipping
        float feedback = FastMath::fastTanh(ladderStage_[3] * resCoeff);
        float u = input - feedback;

        // 4 cascaded non-linear 1-pole stages
        float stage0_in = FastMath::fastTanh(u);
        ladderStage_[0] += g * (stage0_in - ladderStage_[0]);

        float stage1_in = FastMath::fastTanh(ladderStage_[0]);
        ladderStage_[1] += g * (stage1_in - ladderStage_[1]);

        float stage2_in = FastMath::fastTanh(ladderStage_[1]);
        ladderStage_[2] += g * (stage2_in - ladderStage_[2]);

        float stage3_in = FastMath::fastTanh(ladderStage_[2]);
        ladderStage_[3] += g * (stage3_in - ladderStage_[3]);

        // Anti-denormal flush
        for (float& stage : ladderStage_) {
            if (std::abs(stage) < 1.0e-15f) stage = 0.0f;
        }

        return ladderStage_[3];
    }

    // Feedback Comb Filter
    inline float processComb(float input, float polarity) {
        // Calculate delay in samples from pitch/frequency
        float delaySamples = std::clamp(sampleRate_ / cutoffHz_, 2.0f, static_cast<float>(COMB_MAX_DELAY - 2));
        
        // Read delay tap with linear interpolation
        float readPos = static_cast<float>(combWritePos_) - delaySamples;
        if (readPos < 0.0f) {
            readPos += static_cast<float>(COMB_MAX_DELAY);
        }

        size_t idx0 = static_cast<size_t>(readPos);
        size_t idx1 = (idx0 + 1) % COMB_MAX_DELAY;
        float frac = readPos - static_cast<float>(idx0);

        float delayed = combBuffer_[idx0] + frac * (combBuffer_[idx1] - combBuffer_[idx0]);

        // Feedback amount from resonance (0.0 to 0.98)
        float feedbackAmount = resonance_ * 0.98f * polarity;
        float output = input + delayed;

        // Write to comb circular buffer
        combBuffer_[combWritePos_] = input + delayed * feedbackAmount;
        combWritePos_ = (combWritePos_ + 1) % COMB_MAX_DELAY;

        return output * 0.5f;
    }

    FilterType type_{FLT_BYPASS};
    float sampleRate_{44100.0f};
    float cutoffHz_{20000.0f};
    float resonance_{0.0f};
    float drive_{0.0f};

    // SVF state
    float s1_{0.0f};
    float s2_{0.0f};

    // 4-pole Ladder state
    std::array<float, 4> ladderStage_{0.0f, 0.0f, 0.0f, 0.0f};

    // Comb filter state
    std::array<float, COMB_MAX_DELAY> combBuffer_{};
    size_t combWritePos_{0};
};

} // namespace toad
