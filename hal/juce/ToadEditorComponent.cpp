#include "ToadEditorComponent.h"

#if defined(TOAD_ENABLE_JUCE) || __has_include(<juce_gui_basics/juce_gui_basics.h>)

namespace toad {

ToadEditorComponent::ToadEditorComponent(ToadAudioProcessor& processor, int scale)
    : AudioProcessorEditor(processor),
      processor_(processor),
      scale_(scale),
      frameImage_(juce::Image::ARGB,
                  static_cast<int>(DisplayEngine::SCREEN_WIDTH),
                  static_cast<int>(DisplayEngine::SCREEN_HEIGHT),
                  true)
{
    setSize(static_cast<int>(DisplayEngine::SCREEN_WIDTH) * scale_,
            static_cast<int>(DisplayEngine::SCREEN_HEIGHT) * scale_);

    setWantsKeyboardFocus(true);
    addKeyListener(this);
    startTimerHz(60); // 60 Hz display refresh
}

ToadEditorComponent::~ToadEditorComponent() {
    stopTimer();
    removeKeyListener(this);
}

void ToadEditorComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF081008));

    // Render scaled frame
    if (frameImage_.isValid()) {
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImage(frameImage_, getLocalBounds().toFloat());
    }
}

void ToadEditorComponent::resized() {
}

void ToadEditorComponent::syncPlayhead() {
    PlayheadEvent ev;
    while (processor_.getPlayheadFifo().pop(ev)) {
        for (size_t t = 0; t < MAX_TRACKS; ++t) {
            uiState_.vu_levels[t] = ev.peak_levels[t];
        }
    }
}

void ToadEditorComponent::timerCallback() {
    syncPlayhead();

    // Render UI frame to internal display buffer
    displayEngine_.render(uiState_, processor_.getSong(), processor_.getEngine());

    // Blit to juce::Image
    const uint32_t* argb = displayEngine_.getArgbBuffer();
    juce::Image::BitmapData bmp(frameImage_, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < static_cast<int>(DisplayEngine::SCREEN_HEIGHT); ++y) {
        auto* line = reinterpret_cast<uint32_t*>(bmp.getLinePointer(y));
        std::copy(argb + y * DisplayEngine::SCREEN_WIDTH,
                  argb + (y + 1) * DisplayEngine::SCREEN_WIDTH,
                  line);
    }

    repaint();
}

bool ToadEditorComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) {
    (void)originatingComponent;

    InputEvent ev;
    ev.pressed = true;

    if (key.isKeyCode(juce::KeyPress::upKey)) ev.input = INPUT_UP;
    else if (key.isKeyCode(juce::KeyPress::downKey)) ev.input = INPUT_DOWN;
    else if (key.isKeyCode(juce::KeyPress::leftKey)) ev.input = INPUT_LEFT;
    else if (key.isKeyCode(juce::KeyPress::rightKey)) ev.input = INPUT_RIGHT;
    else if (key.isKeyCode(juce::KeyPress::tabKey)) {
        ev.input = key.getModifiers().isShiftDown() ? INPUT_PAGE_PREV : INPUT_PAGE_NEXT;
    }
    else if (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z') ev.input = INPUT_BTN_A;
    else if (key.getKeyCode() == 'X' || key.getKeyCode() == 'x') ev.input = INPUT_BTN_B;
    else if (key.getKeyCode() == 'A' || key.getKeyCode() == 'a') ev.input = INPUT_BTN_OPT;
    else if (key.getKeyCode() == 'S' || key.getKeyCode() == 's') ev.input = INPUT_BTN_EDIT;
    else if (key.isKeyCode(juce::KeyPress::spaceKey)) ev.input = INPUT_TRANSPORT;
    else if (key.getKeyCode() == 'R' || key.getKeyCode() == 'r') ev.input = INPUT_RECORD;
    else if (key.isKeyCode(juce::KeyPress::pageUpKey)) ev.input = INPUT_OCTAVE_UP;
    else if (key.isKeyCode(juce::KeyPress::pageDownKey)) ev.input = INPUT_OCTAVE_DOWN;
    else if (key.getKeyCode() == 'M' || key.getKeyCode() == 'm') ev.input = INPUT_TRACK_MUTE;
    else if (key.getKeyCode() == 'K' || key.getKeyCode() == 'k') ev.input = INPUT_TRACK_SOLO;
    else return false;

    uiState_.handleInput(ev, processor_.getSong(), processor_.getEngine());
    return true;
}

} // namespace toad

#endif
