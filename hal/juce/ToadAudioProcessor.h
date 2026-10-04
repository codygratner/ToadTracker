#pragma once

#include "toad/engine.h"
#include "toad/ui_state.h"
#include "toad/display_engine.h"
#include "../common/hal_interface.h"
#include "JuceMidiOutputSink.h"

#if defined(TOAD_ENABLE_JUCE) || __has_include(<juce_audio_processors/juce_audio_processors.h>)
#include <juce_audio_processors/juce_audio_processors.h>

namespace toad {

// Playhead event communicating audio-thread state to UI editor
struct PlayheadEvent {
    uint8_t song_row{0};
    uint8_t phrase_step[MAX_TRACKS]{};
    float peak_levels[MAX_TRACKS]{};
    bool is_playing{false};
};

class ToadAudioProcessor : public juce::AudioProcessor {
public:
    ToadAudioProcessor();
    ~ToadAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ToadTracker"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { (void)index; }
    const juce::String getProgramName(int index) override { (void)index; return {}; }
    void changeProgramName(int index, const juce::String& newName) override { (void)index; (void)newName; }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    Engine& getEngine() { return engine_; }
    Song& getSong() { return song_; }
    SpscRingBuffer<PlayheadEvent, 64>& getPlayheadFifo() { return playheadFifo_; }

private:
    Engine engine_;
    Song song_;
    SpscRingBuffer<PlayheadEvent, 64> playheadFifo_;

    alignas(64) float tempLeftBuffer_[2048]{};
    alignas(64) float tempRightBuffer_[2048]{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToadAudioProcessor)
};

} // namespace toad

#else

namespace toad {

struct PlayheadEvent {
    uint8_t song_row{0};
    uint8_t phrase_step[MAX_TRACKS]{};
    float peak_levels[MAX_TRACKS]{};
    bool is_playing{false};
};

// Standalone headless desktop audio processor harness
class ToadAudioProcessor {
public:
    ToadAudioProcessor() {
        engine_.loadSong(song_);
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) {
        engine_.setSampleRate(static_cast<float>(sampleRate));
        (void)samplesPerBlock;
    }

    void processBlockHeadless(float* left, float* right, size_t numSamples, IMidiOutputSink* midiSink = nullptr) {
        if (!left || !right || numSamples == 0) return;
        engine_.renderVoices(left, right, numSamples);

        // Push status to FIFO
        PlayheadEvent ev;
        ev.song_row = engine_.getSongRow();
        ev.is_playing = engine_.isPlaying();
        for (size_t t = 0; t < MAX_TRACKS; ++t) {
            ev.phrase_step[t] = engine_.getTrackState(t).phrase_step;
            ev.peak_levels[t] = engine_.getTrackState(t).effective_volume / 255.0f;
        }
        playheadFifo_.push(ev);
    }

    Engine& getEngine() { return engine_; }
    Song& getSong() { return song_; }
    SpscRingBuffer<PlayheadEvent, 64>& getPlayheadFifo() { return playheadFifo_; }

private:
    Engine engine_;
    Song song_;
    SpscRingBuffer<PlayheadEvent, 64> playheadFifo_;
};

} // namespace toad

#endif
