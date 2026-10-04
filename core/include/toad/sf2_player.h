#pragma once

#include "types.h"
#include "fast_math.h"
#include "filter.h"
#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>

namespace toad {

constexpr size_t MAX_SF2_ZONES = 64;

struct SF2SampleZone {
    uint8_t  low_key{0};
    uint8_t  high_key{127};
    uint8_t  low_velocity{0};
    uint8_t  high_velocity{127};
    uint32_t sample_start{0};
    uint32_t sample_end{0};
    uint32_t loop_start{0};
    uint32_t loop_end{0};
    uint32_t sample_rate{44100};
    uint8_t  root_key{60};
    int8_t   fine_tune{0};
    bool     is_looped{false};
};

struct SF2InstrumentData {
    uint8_t zone_count{0};
    std::array<SF2SampleZone, MAX_SF2_ZONES> zones{};
    const int16_t* pcm_data{nullptr};
    size_t pcm_sample_count{0};
};

class SF2Voice {
public:
    SF2Voice() = default;

    void setSampleRate(float sampleRateHz) {
        if (sampleRateHz > 0.0f) {
            sampleRate_ = sampleRateHz;
            filter_.setSampleRate(sampleRateHz);
            reset();
        }
    }

    void reset() {
        active_ = false;
        samplePos_ = 0.0;
        step_ = 0.0;
        activeZone_ = nullptr;
        pcmData_ = nullptr;
        filter_.reset();
    }

    void setInstrumentData(const SF2InstrumentData* data) {
        instData_ = data;
    }

    void noteOn(uint8_t note, uint8_t velocity, const Instrument& inst) {
        if (!instData_ || instData_->zone_count == 0 || !instData_->pcm_data) {
            return;
        }

        // Find matching key/velocity zone
        const SF2SampleZone* matched = nullptr;
        for (uint8_t z = 0; z < instData_->zone_count; ++z) {
            const auto& zone = instData_->zones[z];
            if (note >= zone.low_key && note <= zone.high_key &&
                velocity >= zone.low_velocity && velocity <= zone.high_velocity) {
                matched = &zone;
                break;
            }
        }

        if (!matched) {
            // Default to zone 0 if no exact key split matched
            matched = &instData_->zones[0];
        }

        activeZone_ = matched;
        pcmData_ = instData_->pcm_data;
        pcmLength_ = instData_->pcm_sample_count;

        samplePos_ = static_cast<double>(activeZone_->sample_start);
        float pitchOffset = static_cast<float>(note) - static_cast<float>(activeZone_->root_key) +
                            (static_cast<float>(activeZone_->fine_tune) / 100.0f);
        float pitchRatio = FastMath::semitoneToRatio(pitchOffset);
        float rateRatio = static_cast<float>(activeZone_->sample_rate) / sampleRate_;
        step_ = static_cast<double>(pitchRatio * rateRatio);

        gain_ = static_cast<float>(velocity) / 255.0f;
        active_ = true;

        filter_.setParameters(inst.filter_type, inst.filter_cutoff, inst.filter_resonance, inst.drive_amount);
    }

    void noteOff() {
        if (activeZone_ && !activeZone_->is_looped) {
            active_ = false;
        }
    }

    bool isActive() const { return active_; }

    void setPitch(float note) {
        if (!activeZone_) return;
        float pitchOffset = note - static_cast<float>(activeZone_->root_key) +
                            (static_cast<float>(activeZone_->fine_tune) / 100.0f);
        float pitchRatio = FastMath::semitoneToRatio(pitchOffset);
        float rateRatio = static_cast<float>(activeZone_->sample_rate) / sampleRate_;
        step_ = static_cast<double>(pitchRatio * rateRatio);
    }

    void setFilterParameters(FilterType type, float cutoffHz, float resonance, float drive = 0.0f) {
        filter_.setParameters(type, cutoffHz, resonance, drive);
    }

    inline float process() {
        if (!active_ || !pcmData_ || !activeZone_) {
            return 0.0f;
        }

        size_t idx0 = static_cast<size_t>(samplePos_);
        if (idx0 >= pcmLength_ || idx0 >= activeZone_->sample_end) {
            if (activeZone_->is_looped && activeZone_->loop_end > activeZone_->loop_start) {
                samplePos_ = static_cast<double>(activeZone_->loop_start);
                idx0 = activeZone_->loop_start;
            } else {
                active_ = false;
                return 0.0f;
            }
        }

        size_t idx1 = (idx0 + 1 < pcmLength_) ? idx0 + 1 : idx0;
        float frac = static_cast<float>(samplePos_ - static_cast<double>(idx0));

        // Read 16-bit linear PCM and convert to normalized float [-1.0, 1.0]
        float s0 = static_cast<float>(pcmData_[idx0]) / 32768.0f;
        float s1 = static_cast<float>(pcmData_[idx1]) / 32768.0f;
        float sample = s0 + frac * (s1 - s0);

        samplePos_ += step_;

        // Loop wrap-around
        if (activeZone_->is_looped && activeZone_->loop_end > activeZone_->loop_start) {
            if (samplePos_ >= static_cast<double>(activeZone_->loop_end)) {
                double loopLen = static_cast<double>(activeZone_->loop_end - activeZone_->loop_start);
                samplePos_ -= loopLen;
            }
        }

        float filtered = filter_.process(sample);
        return filtered * gain_;
    }

private:
    float sampleRate_{44100.0f};
    bool active_{false};

    const SF2InstrumentData* instData_{nullptr};
    const SF2SampleZone* activeZone_{nullptr};
    const int16_t* pcmData_{nullptr};
    size_t pcmLength_{0};

    double samplePos_{0.0};
    double step_{0.0};
    float gain_{0.0f};

    UniversalFilter filter_;
};

} // namespace toad
