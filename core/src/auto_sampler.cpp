#include "toad/auto_sampler.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace toad {

bool AutoSampler::findZeroCrossingLoop(const float* buffer, size_t totalSamples,
                                      size_t searchStart, size_t& outLoopStart, size_t& outLoopEnd) {
    if (!buffer || totalSamples < 1024 || searchStart >= totalSamples - 512) {
        return false;
    }

    size_t minLoopLen = 256;
    size_t searchEnd = totalSamples - 128;

    // Find first positive-going zero crossing after searchStart
    size_t startCandidate = 0;
    for (size_t i = searchStart; i < searchEnd - minLoopLen; ++i) {
        if (buffer[i] <= 0.0f && buffer[i + 1] > 0.0f) {
            startCandidate = i;
            break;
        }
    }

    if (startCandidate == 0) return false;

    // Find best matching zero crossing for loopEnd
    size_t bestEnd = 0;
    float bestError = 1e9f;

    for (size_t j = startCandidate + minLoopLen; j < searchEnd; ++j) {
        if (buffer[j] <= 0.0f && buffer[j + 1] > 0.0f) {
            // Compute correlation error over 32 samples
            float err = 0.0f;
            for (size_t k = 0; k < 32 && (startCandidate + k < searchEnd) && (j + k < totalSamples); ++k) {
                float diff = buffer[startCandidate + k] - buffer[j + k];
                err += diff * diff;
            }

            if (err < bestError) {
                bestError = err;
                bestEnd = j;
                if (err < 0.001f) break; // Close enough match
            }
        }
    }

    if (bestEnd > startCandidate) {
        outLoopStart = static_cast<uint32_t>(startCandidate);
        outLoopEnd = static_cast<uint32_t>(bestEnd);
        return true;
    }

    return false;
}

BakedSample AutoSampler::bakeNote(Engine& engine, uint8_t instrumentId, uint8_t note,
                                 float noteDurationSec, float releaseSec, uint32_t sampleRate, bool detectLoop) {
    BakedSample sample;
    sample.root_note = note;
    sample.sample_rate = sampleRate;

    engine.setSampleRate(static_cast<float>(sampleRate));
    const Instrument& inst = engine.getSong().instruments[instrumentId < TOTAL_INSTRUMENTS ? instrumentId : 0];

    size_t noteSamples = static_cast<size_t>(noteDurationSec * sampleRate);
    size_t releaseSamples = static_cast<size_t>(releaseSec * sampleRate);
    size_t totalSamples = noteSamples + releaseSamples;

    sample.pcm_data.resize(totalSamples);
    std::vector<float> renderBuffer(totalSamples, 0.0f);

    // Create a dedicated synth voice instance to render without modifying live engine state
    SynthVoice synthVoice;
    WavetableVoice wtVoice;
    synthVoice.setSampleRate(static_cast<float>(sampleRate));
    wtVoice.setSampleRate(static_cast<float>(sampleRate));

    if (inst.type == INST_TYPE_INTERNAL_SYNTH) {
        synthVoice.noteOn(note, 0xFF, inst.synth, inst);
    } else if (inst.type == INST_TYPE_WAVETABLE) {
        wtVoice.noteOn(note, 0xFF, inst.wavetable, inst);
    }

    // Render in blocks of 64
    size_t processed = 0;
    while (processed < totalSamples) {
        size_t slice = std::min(static_cast<size_t>(64), totalSamples - processed);

        if (processed >= noteSamples) {
            // Note release phase
            if (inst.type == INST_TYPE_INTERNAL_SYNTH) synthVoice.noteOff();
            else if (inst.type == INST_TYPE_WAVETABLE) wtVoice.noteOff();
        }

        for (size_t i = 0; i < slice; ++i) {
            if (inst.type == INST_TYPE_INTERNAL_SYNTH) {
                renderBuffer[processed + i] = synthVoice.process();
            } else if (inst.type == INST_TYPE_WAVETABLE) {
                renderBuffer[processed + i] = wtVoice.process();
            }
        }

        processed += slice;
    }

    // Convert float (-1.0 .. +1.0) to 16-bit PCM
    for (size_t i = 0; i < totalSamples; ++i) {
        float s = FastMath::masterClip(renderBuffer[i]);
        int32_t val = static_cast<int32_t>(s * 32767.0f);
        val = std::clamp(val, -32768, 32767);
        sample.pcm_data[i] = static_cast<int16_t>(val);
    }

    // Loop detection
    if (detectLoop && noteSamples > 2048) {
        size_t searchStart = noteSamples / 2;
        size_t lStart = 0, lEnd = 0;
        if (findZeroCrossingLoop(renderBuffer.data(), noteSamples, searchStart, lStart, lEnd)) {
            sample.loop_start = static_cast<uint32_t>(lStart);
            sample.loop_end = static_cast<uint32_t>(lEnd);
            sample.is_looped = true;
        }
    }

    return sample;
}

std::vector<BakedSample> AutoSampler::bakeInstrument(Engine& engine, uint8_t instrumentId,
                                                    const AutoSamplerConfig& config) {
    std::vector<BakedSample> result;
    uint8_t step = (config.semitone_step > 0) ? config.semitone_step : 6;

    for (uint8_t note = config.note_start; note <= config.note_end; note += step) {
        result.push_back(bakeNote(engine, instrumentId, note,
                                  config.note_duration_sec,
                                  config.release_sec,
                                  config.sample_rate,
                                  config.detect_loops));
        if (note + step < note) break; // Overflow protection
    }

    return result;
}

std::vector<uint8_t> AutoSampler::createWavFile(const BakedSample& sample) {
    std::vector<uint8_t> out;

    uint32_t numSamples = static_cast<uint32_t>(sample.pcm_data.size());
    uint32_t dataBytes = numSamples * sizeof(int16_t);
    uint32_t sampleRate = sample.sample_rate;
    uint16_t numChannels = 1;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    uint16_t blockAlign = numChannels * (bitsPerSample / 8);

    // Compute total RIFF size
    uint32_t riffSize = 36 + dataBytes;
    if (sample.is_looped) {
        riffSize += 68; // 'smpl' chunk size
    }

    // Write RIFF Header
    auto writeBytes = [&](const void* ptr, size_t count) {
        const auto* b = reinterpret_cast<const uint8_t*>(ptr);
        out.insert(out.end(), b, b + count);
    };

    writeBytes("RIFF", 4);
    writeBytes(&riffSize, 4);
    writeBytes("WAVE", 4);

    // 'fmt ' chunk
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1; // PCM
    writeBytes("fmt ", 4);
    writeBytes(&fmtSize, 4);
    writeBytes(&audioFormat, 2);
    writeBytes(&numChannels, 2);
    writeBytes(&sampleRate, 4);
    writeBytes(&byteRate, 4);
    writeBytes(&blockAlign, 2);
    writeBytes(&bitsPerSample, 2);

    // 'smpl' chunk (sampler loop metadata)
    if (sample.is_looped) {
        uint32_t smplSize = 60; // 36 header bytes + 24 loop definition bytes
        uint32_t manufacturer = 0;
        uint32_t product = 0;
        uint32_t samplePeriod = 1000000000 / sampleRate;
        uint32_t midiUnityNote = sample.root_note;
        uint32_t midiPitchFraction = 0;
        uint32_t smpteFormat = 0;
        uint32_t smpteOffset = 0;
        uint32_t numSampleLoops = 1;
        uint32_t samplerData = 0;

        writeBytes("smpl", 4);
        writeBytes(&smplSize, 4);
        writeBytes(&manufacturer, 4);
        writeBytes(&product, 4);
        writeBytes(&samplePeriod, 4);
        writeBytes(&midiUnityNote, 4);
        writeBytes(&midiPitchFraction, 4);
        writeBytes(&smpteFormat, 4);
        writeBytes(&smpteOffset, 4);
        writeBytes(&numSampleLoops, 4);
        writeBytes(&samplerData, 4);

        // Loop definition
        uint32_t loopCuePointID = 0;
        uint32_t loopType = 0; // Forward loop
        uint32_t loopStart = sample.loop_start;
        uint32_t loopEnd = sample.loop_end;
        uint32_t loopFraction = 0;
        uint32_t loopPlayCount = 0; // Infinite

        writeBytes(&loopCuePointID, 4);
        writeBytes(&loopType, 4);
        writeBytes(&loopStart, 4);
        writeBytes(&loopEnd, 4);
        writeBytes(&loopFraction, 4);
        writeBytes(&loopPlayCount, 4);
    }

    // 'data' chunk
    writeBytes("data", 4);
    writeBytes(&dataBytes, 4);
    writeBytes(sample.pcm_data.data(), dataBytes);

    return out;
}

} // namespace toad
