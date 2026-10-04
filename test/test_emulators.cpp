#include <catch2/catch_test_macros.hpp>
#include <toad/engine.h>
#include <toad/display_engine.h>
#include <toad/ui_state.h>
#include <hal/esp32/esp32_harness.h>
#include <hal/steamdeck/steamdeck_harness.h>
#include <apps/common/demo_song.h>

#include <vector>
#include <memory>
#include <cmath>

TEST_CASE("Emulators: Demo Song Initialization & Audio Rendering", "[emulators]") {
    auto demo = std::make_unique<toad::Song>(toad::createDemoSong());

    REQUIRE(demo->bpm == 128.0f);
    REQUIRE(demo->groove == 6);
    REQUIRE(demo->master_scale.enabled == true);

    // Verify Instruments
    REQUIRE(demo->instruments[0].type == toad::INST_TYPE_INTERNAL_SYNTH);
    REQUIRE(demo->instruments[1].type == toad::INST_TYPE_WAVETABLE);
    REQUIRE(demo->instruments[2].type == toad::INST_TYPE_INTERNAL_SYNTH);
    REQUIRE(demo->instruments[3].type == toad::INST_TYPE_INTERNAL_SYNTH);

    auto engine = std::make_unique<toad::Engine>();
    engine->setSampleRate(44100.0f);
    engine->loadSong(*demo);
    engine->play(toad::PLAY_SONG);

    REQUIRE(engine->isPlaying());

    // Render 1024 frames of audio (2048 float samples)
    std::vector<float> audioBuffer(1024 * 2, 0.0f);
    engine->renderBlockDeterministic(audioBuffer.data(), 1024);

    float energy = 0.0f;
    for (float sample : audioBuffer) {
        REQUIRE_FALSE(std::isnan(sample));
        REQUIRE_FALSE(std::isinf(sample));
        // Hard master clip guarantee
        REQUIRE(std::abs(sample) <= 1.0f);
        energy += std::abs(sample);
    }
    // Must generate actual synthesized sound
    REQUIRE(energy > 0.1f);
}

TEST_CASE("Emulators: Display Engine Views and Steam Deck Compositing", "[emulators]") {
    auto demo = std::make_unique<toad::Song>(toad::createDemoSong());
    auto engine = std::make_unique<toad::Engine>();
    engine->setSampleRate(44100.0f);
    engine->loadSong(*demo);

    toad::UIState ui;
    auto display = std::make_unique<toad::DisplayEngine>();

    SECTION("All UI views render valid 240x240 frame buffers without crashes") {
        toad::TrackerView views[] = {
            toad::VIEW_SONG,
            toad::VIEW_CHAIN,
            toad::VIEW_PHRASE,
            toad::VIEW_TABLE,
            toad::VIEW_INSTRUMENT,
            toad::VIEW_SYNTH,
            toad::VIEW_PROJECT
        };

        for (auto view : views) {
            ui.current_view = view;
            display->render(ui, *demo, *engine);

            const uint32_t* argb = display->getArgbBuffer();
            const uint16_t* rgb565 = display->getRgb565Buffer();

            REQUIRE(argb != nullptr);
            REQUIRE(rgb565 != nullptr);

            // Verify non-empty frame
            bool hasContent = false;
            for (size_t i = 0; i < toad::DisplayEngine::TOTAL_PIXELS; ++i) {
                if (argb[i] != toad::DisplayEngine::Colors::BG_OBSIDIAN) {
                    hasContent = true;
                    break;
                }
            }
            REQUIRE(hasContent);
        }
    }

    SECTION("Steam Deck 1280x800 compositing correctly places 3x scaled center canvas") {
        ui.current_view = toad::VIEW_SONG;
        display->render(ui, *demo, *engine);

        std::vector<uint32_t> composited(toad::DisplayEngine::STEAM_DECK_WIDTH * toad::DisplayEngine::STEAM_DECK_HEIGHT, 0);
        display->compositeSteamDeck(composited.data(), ui, *engine);

        // Check center pixel of the 720x720 scaled viewport (at X = 280 + 360 = 640, Y = 40 + 360 = 400)
        size_t centerIdx = 400 * toad::DisplayEngine::STEAM_DECK_WIDTH + 640;
        REQUIRE(composited[centerIdx] != 0);

        // Check lateral flank borders
        size_t leftBorderIdx = 100 * toad::DisplayEngine::STEAM_DECK_WIDTH + (toad::DisplayEngine::STEAM_DECK_OFFSET_X - 1);
        REQUIRE(composited[leftBorderIdx] == toad::DisplayEngine::Colors::GRID_LINE);
    }
}

TEST_CASE("Emulators: TBD-16 NeoPixel Step LEDs and Input Mapping", "[emulators]") {
    toad::ESP32HAL hal;
    hal.initialize(44100, 512);

    SECTION("Step LEDs update with appropriate playhead and cursor colors") {
        // Active step = 2 (Cyan), Playing step = 5 (Green), Downbeat = 0 (Amber)
        hal.updateStepLeds(2, 5, true);
        const auto& leds = hal.getStepLeds();

        // Downbeat 0
        REQUIRE(leds[0].r == 40);
        REQUIRE(leds[0].g == 25);
        REQUIRE(leds[0].b == 0);

        // Active edit step 2 (Cyan)
        REQUIRE(leds[2].r == 0);
        REQUIRE(leds[2].g == 180);
        REQUIRE(leds[2].b == 255);

        // Playing step 5 (Bright)
        REQUIRE(leds[5].r == 200);
        REQUIRE(leds[5].g == 255);
        REQUIRE(leds[5].b == 200);
    }
}
