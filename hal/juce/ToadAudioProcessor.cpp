#include "ToadAudioProcessor.h"

#if defined(TOAD_ENABLE_JUCE) || __has_include(<juce_audio_processors/juce_audio_processors.h>)

namespace toad {

ToadAudioProcessor::ToadAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    engine_.loadSong(song_);
}

ToadAudioProcessor::~ToadAudioProcessor() = default;

void ToadAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    (void)samplesPerBlock;
    engine_.setSampleRate(static_cast<float>(sampleRate));
}

void ToadAudioProcessor::releaseResources() {
    engine_.stop();
}

bool ToadAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
        return false;
    }
    return true;
}

void ToadAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    const int totalNumSamples = buffer.getNumSamples();
    if (totalNumSamples <= 0) return;

    // Detect non-realtime / offline export
    bool isOffline = false;
    if (auto* playHead = getPlayHead()) {
        if (auto pos = playHead->getPosition()) {
            isOffline = pos->getIsNonRealtime();
        }
    }

    // Connect MIDI output sink
    JuceMidiOutputSink midiSink(midiMessages);

    // Sliced deterministic render
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = (buffer.getNumChannels() > 1) ? buffer.getWritePointer(1) : nullptr;

    size_t samplesProcessed = 0;
    while (samplesProcessed < static_cast<size_t>(totalNumSamples)) {
        size_t samplesUntilTick = engine_.getSamplesUntilNextTick();
        size_t slice = std::min(static_cast<size_t>(totalNumSamples) - samplesProcessed,
                                samplesUntilTick > 0 ? samplesUntilTick : static_cast<size_t>(totalNumSamples));

        engine_.renderVoices(leftChannel + samplesProcessed, slice);

        if (rightChannel) {
            std::copy(leftChannel + samplesProcessed,
                      leftChannel + samplesProcessed + slice,
                      rightChannel + samplesProcessed);
        }

        samplesProcessed += slice;
        engine_.advanceSampleClock(slice);
    }

    // Only post to UI FIFO if not rendering offline
    if (!isOffline) {
        PlayheadEvent ev;
        ev.song_row = engine_.getSongRow();
        ev.is_playing = engine_.isPlaying();
        for (size_t t = 0; t < MAX_TRACKS; ++t) {
            ev.phrase_step[t] = engine_.getTrackState(t).phrase_step;
            ev.peak_levels[t] = engine_.getTrackState(t).effective_volume / 255.0f;
        }
        playheadFifo_.push(ev);
    }
}

juce::AudioProcessorEditor* ToadAudioProcessor::createEditor() {
    return nullptr; // Custom editor attached in ToadEditorComponent
}

void ToadAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    (void)destData;
}

void ToadAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    (void)data;
    (void)sizeInBytes;
}

} // namespace toad

#endif
