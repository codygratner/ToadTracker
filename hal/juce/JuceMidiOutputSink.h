#pragma once

#include "../common/hal_interface.h"

// If JUCE is available, include JUCE headers; otherwise forward declare for headless analysis
#if defined(TOAD_ENABLE_JUCE) || __has_include(<juce_audio_processors/juce_audio_processors.h>)
#include <juce_audio_basics/juce_audio_basics.h>

namespace toad {

class JuceMidiOutputSink : public IMidiOutputSink {
public:
    explicit JuceMidiOutputSink(juce::MidiBuffer& targetBuffer)
        : targetBuffer_(targetBuffer) {}

    void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t sampleOffset) override {
        targetBuffer_.addEvent(juce::MidiMessage::noteOn(static_cast<int>(channel + 1),
                                                         static_cast<int>(note),
                                                         static_cast<juce::uint8>(velocity)),
                               static_cast<int>(sampleOffset));
    }

    void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t sampleOffset) override {
        targetBuffer_.addEvent(juce::MidiMessage::noteOff(static_cast<int>(channel + 1),
                                                          static_cast<int>(note),
                                                          static_cast<juce::uint8>(velocity)),
                               static_cast<int>(sampleOffset));
    }

    void sendControlChange(uint8_t channel, uint8_t controller, uint8_t value, uint32_t sampleOffset) override {
        targetBuffer_.addEvent(juce::MidiMessage::controllerEvent(static_cast<int>(channel + 1),
                                                                  static_cast<int>(controller),
                                                                  static_cast<int>(value)),
                               static_cast<int>(sampleOffset));
    }

    void sendPitchBend(uint8_t channel, uint16_t bendValue, uint32_t sampleOffset) override {
        targetBuffer_.addEvent(juce::MidiMessage::pitchWheel(static_cast<int>(channel + 1),
                                                             static_cast<int>(bendValue)),
                               static_cast<int>(sampleOffset));
    }

    void clear() override {
        targetBuffer_.clear();
    }

private:
    juce::MidiBuffer& targetBuffer_;
};

} // namespace toad

#else

namespace toad {

// Headless fallback sink for testing environments without JUCE SDK
class JuceMidiOutputSink : public IMidiOutputSink {
public:
    void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t sampleOffset) override {
        (void)channel; (void)note; (void)velocity; (void)sampleOffset;
    }
    void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint32_t sampleOffset) override {
        (void)channel; (void)note; (void)velocity; (void)sampleOffset;
    }
    void sendControlChange(uint8_t channel, uint8_t controller, uint8_t value, uint32_t sampleOffset) override {
        (void)channel; (void)controller; (void)value; (void)sampleOffset;
    }
    void sendPitchBend(uint8_t channel, uint16_t bendValue, uint32_t sampleOffset) override {
        (void)channel; (void)bendValue; (void)sampleOffset;
    }
    void clear() override {}
};

} // namespace toad

#endif
