#pragma once

#include "ToadAudioProcessor.h"
#include "toad/display_engine.h"
#include "toad/ui_state.h"

#if defined(TOAD_ENABLE_JUCE) || __has_include(<juce_gui_basics/juce_gui_basics.h>)
#include <juce_gui_basics/juce_gui_basics.h>

namespace toad {

class ToadEditorComponent : public juce::AudioProcessorEditor,
                            public juce::Timer,
                            public juce::KeyListener {
public:
    explicit ToadEditorComponent(ToadAudioProcessor& processor, int scale = 2);
    ~ToadEditorComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

private:
    ToadAudioProcessor& processor_;
    int scale_{2};
    UIState uiState_;
    DisplayEngine displayEngine_;
    juce::Image frameImage_;

    void syncPlayhead();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToadEditorComponent)
};

} // namespace toad

#else

namespace toad {

// Standalone desktop editor component harness for non-JUCE test environments
class ToadEditorComponent {
public:
    explicit ToadEditorComponent(ToadAudioProcessor& processor, int scale = 2)
        : processor_(processor), scale_(scale) {}

    void tick60Hz() {
        // Drain playhead FIFO
        PlayheadEvent ev;
        while (processor_.getPlayheadFifo().pop(ev)) {
            for (size_t t = 0; t < MAX_TRACKS; ++t) {
                uiState_.vu_levels[t] = ev.peak_levels[t];
            }
        }
        displayEngine_.render(uiState_, processor_.getSong(), processor_.getEngine());
    }

    void handleKey(LogicalInput input, bool pressed = true) {
        InputEvent ev;
        ev.input = input;
        ev.pressed = pressed;
        uiState_.handleInput(ev, processor_.getSong(), processor_.getEngine());
    }

    [[nodiscard]] const UIState& getUIState() const noexcept { return uiState_; }
    [[nodiscard]] UIState& getUIState() noexcept { return uiState_; }
    [[nodiscard]] const DisplayEngine& getDisplayEngine() const noexcept { return displayEngine_; }
    [[nodiscard]] int getScale() const noexcept { return scale_; }

private:
    ToadAudioProcessor& processor_;
    int scale_{2};
    UIState uiState_;
    DisplayEngine displayEngine_;
};

} // namespace toad

#endif
