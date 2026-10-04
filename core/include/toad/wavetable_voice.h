#pragma once

#include "types.h"
#include "fast_math.h"
#include "filter.h"
#include <cmath>
#include <array>
#include <algorithm>

namespace toad {

constexpr size_t WT_FRAME_SIZE = 2048;
constexpr size_t WT_MAX_FRAMES = 64;

// Cubic Hermite interpolation between 4 points
inline float cubicHermite(float y0, float y1, float y2, float y3, float t) {
    float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c = -0.5f * y0 + 0.5f * y2;
    float d = y1;
    return a * t * t * t + b * t * t + c * t + d;
}

// Built-in Basic Shapes Wavetable (Frame 0: Sine, Frame 1: Triangle, Frame 2: Saw, Frame 3: Square)
struct WavetableData {
    uint8_t num_frames{4};
    std::array<std::array<float, WT_FRAME_SIZE>, WT_MAX_FRAMES> frames{};

    WavetableData() {
        initDefaultShapes();
    }

    void initDefaultShapes() {
        num_frames = 4;
        for (size_t i = 0; i < WT_FRAME_SIZE; ++i) {
            float phase = static_cast<float>(i) / static_cast<float>(WT_FRAME_SIZE);
            // Frame 0: Sine
            frames[0][i] = std::sin(phase * 6.2831853f);
            // Frame 1: Triangle
            frames[1][i] = (phase < 0.5f) ? (-1.0f + 4.0f * phase) : (3.0f - 4.0f * phase);
            // Frame 2: Saw
            frames[2][i] = 1.0f - 2.0f * phase;
            // Frame 3: Square
            frames[3][i] = (phase < 0.5f) ? 1.0f : -1.0f;
        }
    }
};

class WavetableVoice {
public:
    WavetableVoice() = default;

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
    }

    void setWavetable(const WavetableData* wt) {
        wavetable_ = wt ? wt : &defaultWavetable_;
    }

    void noteOn(uint8_t note, uint8_t velocity, const WavetableConfig& config, const Instrument& inst) {
        note_ = note;
        velocity_ = velocity;
        active_ = true;

        frequencyHz_ = FastMath::noteToFrequency(static_cast<float>(note));
        phaseInc_ = frequencyHz_ / sampleRate_;
        if (phaseInc_ > 0.49f) phaseInc_ = 0.49f;

        position_ = std::clamp(config.position, 0.0f, static_cast<float>(wavetable_->num_frames - 1));
        warpMode_ = config.warp_mode;
        warpAmount_ = std::clamp(config.warp_amount, 0.0f, 1.0f);

        envLevel_ = static_cast<float>(velocity) / 255.0f;
        filter_.setParameters(inst.filter_type, inst.filter_cutoff, inst.filter_resonance, inst.drive_amount);
    }

    void noteOff() {
        active_ = false;
        envLevel_ = 0.0f;
    }

    bool isActive() const { return active_; }
    uint8_t getNote() const { return note_; }

    void setPosition(float pos) {
        position_ = std::clamp(pos, 0.0f, static_cast<float>(wavetable_->num_frames - 1));
    }

    void setWarp(WarpMode mode, float amount) {
        warpMode_ = mode;
        warpAmount_ = std::clamp(amount, 0.0f, 1.0f);
    }

    void setPitch(float note) {
        frequencyHz_ = FastMath::noteToFrequency(note);
        phaseInc_ = frequencyHz_ / sampleRate_;
        if (phaseInc_ > 0.49f) phaseInc_ = 0.49f;
    }

    void setFilterParameters(FilterType type, float cutoffHz, float resonance, float drive = 0.0f) {
        filter_.setParameters(type, cutoffHz, resonance, drive);
    }

    // Phase-Warp Distortion Operators (Section 6.2)
    inline float applyPhaseWarp(float phase) const {
        if (warpMode_ == WARP_OFF || warpAmount_ <= 0.0f) {
            return phase;
        }

        switch (warpMode_) {
            case WARP_PWM: {
                // Squeeze phase into variable pulse width
                float w = 0.5f + (warpAmount_ - 0.5f) * 0.9f;
                w = std::clamp(w, 0.05f, 0.95f);
                if (phase < w) {
                    return (phase / w) * 0.5f;
                } else {
                    return 0.5f + ((phase - w) / (1.0f - w)) * 0.5f;
                }
            }

            case WARP_SYNC: {
                // Windowed phase multiplier (1.0x to 8.0x)
                float syncRatio = 1.0f + warpAmount_ * 7.0f;
                float scaled = phase * syncRatio;
                return scaled - std::floor(scaled);
            }

            case WARP_BEND: {
                // Exponential phase curvature
                float expFactor = (warpAmount_ < 0.5f) ? (1.0f + (0.5f - warpAmount_) * 4.0f) : (1.0f / (1.0f + (warpAmount_ - 0.5f) * 4.0f));
                return std::pow(phase, expFactor);
            }

            case WARP_FOLD: {
                // Mirror reflection of phase
                if (phase < 0.5f) {
                    return phase * 2.0f;
                } else {
                    return (1.0f - phase) * 2.0f;
                }
            }

            case WARP_FORMANT: {
                // Formant squeeze into center of cycle
                float scale = 1.0f + warpAmount_ * 4.0f;
                float scaled = phase * scale;
                return (scaled < 1.0f) ? scaled : 0.0f;
            }

            case WARP_BITCR: {
                // Quantized step staircase phase
                float steps = 4.0f + (1.0f - warpAmount_) * 60.0f;
                return std::floor(phase * steps) / steps;
            }

            default:
                return phase;
        }
    }

    // Process a single audio sample through the wavetable interpolator and filter
    inline float process() {
        if (!active_ || phaseInc_ <= 0.0f) {
            return 0.0f;
        }

        // Apply phase warping
        float warpedPhase = applyPhaseWarp(phase_);

        // Dual-Axis Interpolation:
        // 1. Frame position morphing (inter-frame linear interpolation)
        size_t f0 = static_cast<size_t>(position_);
        size_t f1 = std::min(f0 + 1, static_cast<size_t>(wavetable_->num_frames - 1));
        float frameFrac = position_ - static_cast<float>(f0);

        // 2. Intra-frame sample reading with cubic Hermite interpolation
        float samplePos = warpedPhase * static_cast<float>(WT_FRAME_SIZE);
        size_t i1 = static_cast<size_t>(samplePos);
        size_t i0 = (i1 + WT_FRAME_SIZE - 1) % WT_FRAME_SIZE;
        size_t i2 = (i1 + 1) % WT_FRAME_SIZE;
        size_t i3 = (i1 + 2) % WT_FRAME_SIZE;
        float frac = samplePos - static_cast<float>(i1);

        const auto& frameA = wavetable_->frames[f0];
        float sampleA = cubicHermite(frameA[i0], frameA[i1], frameA[i2], frameA[i3], frac);

        float outputSample = sampleA;
        if (f0 != f1 && frameFrac > 0.0f) {
            const auto& frameB = wavetable_->frames[f1];
            float sampleB = cubicHermite(frameB[i0], frameB[i1], frameB[i2], frameB[i3], frac);
            outputSample = sampleA + frameFrac * (sampleB - sampleA);
        }

        // Advance phase accumulator
        phase_ += phaseInc_;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
        }

        // Filter stage
        float filtered = filter_.process(outputSample);

        return filtered * envLevel_;
    }

private:
    static inline WavetableData defaultWavetable_{};
    const WavetableData* wavetable_{&defaultWavetable_};

    float sampleRate_{44100.0f};
    float frequencyHz_{440.0f};
    float phase_{0.0f};
    float phaseInc_{0.0f};

    float position_{0.0f};
    WarpMode warpMode_{WARP_OFF};
    float warpAmount_{0.0f};

    uint8_t note_{NOTE_EMPTY};
    uint8_t velocity_{0};
    bool active_{false};
    float envLevel_{0.0f};

    UniversalFilter filter_;
};

} // namespace toad
