#include <catch2/catch_test_macros.hpp>
#include "hal/common/hal_interface.h"
#include "hal/steamdeck/steamdeck_harness.h"
#include "hal/juce/ToadAudioProcessor.h"
#include "hal/juce/ToadEditorComponent.h"
#include <vector>

TEST_CASE("HAL: SPSC Lock-Free Ring Buffer Invariants", "[hal]") {
    toad::SpscRingBuffer<uint32_t, 8> ring;

    REQUIRE(ring.isEmpty());
    REQUIRE_FALSE(ring.isFull());
    REQUIRE(ring.size() == 0);

    // Push elements up to capacity
    for (uint32_t i = 0; i < 8; ++i) {
        REQUIRE(ring.push(i + 100));
    }
    REQUIRE(ring.size() == 8);
    REQUIRE(ring.isFull());
    REQUIRE_FALSE(ring.push(999)); // Overflow rejected safely

    // Pop elements in FIFO order
    for (uint32_t i = 0; i < 8; ++i) {
        uint32_t val = 0;
        REQUIRE(ring.pop(val));
        REQUIRE(val == i + 100);
    }
    REQUIRE(ring.isEmpty());

    // Wraparound test over multiple cycles
    for (int cycle = 0; cycle < 10; ++cycle) {
        REQUIRE(ring.push(cycle));
        uint32_t v = 0;
        REQUIRE(ring.pop(v));
        REQUIRE(v == static_cast<uint32_t>(cycle));
    }
}

TEST_CASE("HAL: SteamDeckHAL Initialization and Controller Mapping", "[hal]") {
    toad::SteamDeckHAL deck;

    REQUIRE(deck.initialize(48000, 128));
    REQUIRE(deck.isInitialized());
    REQUIRE(deck.getSampleRate() == 48000);

    // Test Button Injection
    toad::SteamDeckRawInput raw{};
    raw.l1 = true;
    raw.btn_a = true;
    raw.l4_grip = true; // Octave Down
    raw.r4_grip = true; // Mute
    deck.injectRawInput(raw);

    std::vector<toad::LogicalInput> captured;
    deck.pollInput([&](const toad::InputEvent& ev) {
        captured.push_back(ev.input);
    });

    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_PAGE_PREV) != captured.end());
    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_BTN_A) != captured.end());
    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_OCTAVE_DOWN) != captured.end());
    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_TRACK_MUTE) != captured.end());

    // Test Trackpad Rotary Jog Wheel
    raw = {};
    raw.right_trackpad_touch = true;
    raw.right_trackpad_x = 0.8f;
    raw.right_trackpad_y = 0.0f; // Angle = 0
    deck.injectRawInput(raw);
    captured.clear();
    deck.pollInput([&](const toad::InputEvent& ev) { captured.push_back(ev.input); });

    // Rotate trackpad clockwise: X = 0.0, Y = 0.8 (Angle = +pi/2)
    raw.right_trackpad_x = 0.0f;
    raw.right_trackpad_y = 0.8f;
    deck.injectRawInput(raw);
    deck.pollInput([&](const toad::InputEvent& ev) { captured.push_back(ev.input); });

    REQUIRE(std::find(captured.begin(), captured.end(), toad::INPUT_JOG_CW) != captured.end());

    // Test Framebuffer 3x Rendering
    std::vector<uint16_t> fakeFrame(toad::DisplayEngine::SCREEN_WIDTH * toad::DisplayEngine::SCREEN_HEIGHT, 0x07E0); // Green
    deck.renderFrame(fakeFrame.data(), toad::DisplayEngine::SCREEN_WIDTH, toad::DisplayEngine::SCREEN_HEIGHT);
    const uint32_t* composited = deck.getCompositedFrame();
    REQUIRE(composited != nullptr);

    // Test Steam Deck MIDI Output Sink
    toad::IMidiOutputSink* midi = deck.getMidiSink();
    REQUIRE(midi != nullptr);
    midi->sendNoteOn(0, 60, 100, 16);
    midi->sendControlChange(0, 74, 80, 32);
    midi->sendNoteOff(0, 60, 0, 48);

    deck.shutdown();
    REQUIRE_FALSE(deck.isInitialized());
}

TEST_CASE("HAL: Desktop Harness and Editor Component", "[hal]") {
    toad::ToadAudioProcessor processor;
    processor.prepareToPlay(44100.0, 128);

    toad::ToadEditorComponent editor(processor, 2);
    REQUIRE(editor.getScale() == 2);

    // Process a block in headless mode
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    processor.processBlockHeadless(left.data(), right.data(), 256);

    // Run 60 Hz editor tick to consume playhead events
    editor.tick60Hz();

    // Verify key handling
    editor.handleKey(toad::INPUT_UP);
    REQUIRE(editor.getUIState().cursor_row == 0); // Clamped at 0

    editor.handleKey(toad::INPUT_DOWN);
    REQUIRE(editor.getUIState().cursor_row == 1);
}
