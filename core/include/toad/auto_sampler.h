#pragma once

#include "toad/types.h"
#include "toad/engine.h"
#include <cstdint>
#include <cstddef>
#include <vector>

namespace toad {

// ============================================================================
// BAKED MULTI-SAMPLE DATA STRUCTURE
// ============================================================================
struct BakedSample {
    uint8_t  root_note{60};
    uint32_t sample_rate{44100};
    uint32_t loop_start{0};
    uint32_t loop_end{0};
    bool     is_looped{false};
    std::vector<int16_t> pcm_data; // Mono 16-bit linear PCM
};

// ============================================================================
// AUTO-SAMPLER CONFIGURATION
// ============================================================================
struct AutoSamplerConfig {
    uint8_t  note_start{24};       // C1
    uint8_t  note_end{96};         // C7
    uint8_t  semitone_step{6};     // Every 6 semitones (tritones: C, F#, C)
    float    note_duration_sec{1.0f};
    float    release_sec{0.25f};
    uint32_t sample_rate{44100};
    bool     detect_loops{true};
};

// ============================================================================
// AUTO-SAMPLER / INSTRUMENT BOUNCER ENGINE (Section 8.1)
// ============================================================================
class AutoSampler {
public:
    // Zero-crossing phase-aligned loop detection algorithm
    static bool findZeroCrossingLoop(const float* buffer, size_t totalSamples,
                                    size_t searchStart, size_t& outLoopStart, size_t& outLoopEnd);

    // Bake single chromatic note
    static BakedSample bakeNote(Engine& engine, uint8_t instrumentId, uint8_t note,
                               float noteDurationSec, float releaseSec, uint32_t sampleRate, bool detectLoop);

    // Bake complete multi-sample bank across keyboard range
    static std::vector<BakedSample> bakeInstrument(Engine& engine, uint8_t instrumentId,
                                                  const AutoSamplerConfig& config);

    // Write Standard RIFF/WAV Container (16-bit Mono with 'smpl' loop chunk)
    static std::vector<uint8_t> createWavFile(const BakedSample& sample);
};

} // namespace toad
