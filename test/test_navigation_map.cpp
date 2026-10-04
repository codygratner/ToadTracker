#include <catch2/catch_test_macros.hpp>
#include "toad/ui_state.h"
#include "toad/display_engine.h"

TEST_CASE("NavigationMap: 2D Screen Coordinates and View Lookups", "[navmap]") {
    // 3x3 Spatial Grid Layout:
    // Row 0: PROJ(0,0), SETT(1,0)
    // Row 1: SONG(0,1), CHAIN(1,1), PHRASE(2,1)
    // Row 2: INST(0,2), SYNTH(1,2), TABLE(2,2)

    REQUIRE(toad::getScreenCoord(toad::VIEW_PROJECT) == toad::ScreenCoord{0, 0});
    REQUIRE(toad::getScreenCoord(toad::VIEW_SETTINGS) == toad::ScreenCoord{1, 0});
    REQUIRE(toad::getScreenCoord(toad::VIEW_SONG) == toad::ScreenCoord{0, 1});
    REQUIRE(toad::getScreenCoord(toad::VIEW_CHAIN) == toad::ScreenCoord{1, 1});
    REQUIRE(toad::getScreenCoord(toad::VIEW_PHRASE) == toad::ScreenCoord{2, 1});
    REQUIRE(toad::getScreenCoord(toad::VIEW_INSTRUMENT) == toad::ScreenCoord{0, 2});
    REQUIRE(toad::getScreenCoord(toad::VIEW_SYNTH) == toad::ScreenCoord{1, 2});
    REQUIRE(toad::getScreenCoord(toad::VIEW_TABLE) == toad::ScreenCoord{2, 2});

    // Inverse lookup from grid coordinates to TrackerView
    REQUIRE(toad::getViewAtCoord(0, 0) == toad::VIEW_PROJECT);
    REQUIRE(toad::getViewAtCoord(1, 0) == toad::VIEW_SETTINGS);
    REQUIRE(toad::getViewAtCoord(0, 1) == toad::VIEW_SONG);
    REQUIRE(toad::getViewAtCoord(1, 1) == toad::VIEW_CHAIN);
    REQUIRE(toad::getViewAtCoord(2, 1) == toad::VIEW_PHRASE);
    REQUIRE(toad::getViewAtCoord(0, 2) == toad::VIEW_INSTRUMENT);
    REQUIRE(toad::getViewAtCoord(1, 2) == toad::VIEW_SYNTH);
    REQUIRE(toad::getViewAtCoord(2, 2) == toad::VIEW_TABLE);
}

TEST_CASE("NavigationMap: Directional 2D Navigation Transitions", "[navmap]") {
    toad::UIState ui;
    toad::Song song;

    // Start at SONG (0, 1)
    ui.current_view = toad::VIEW_SONG;

    // Move UP -> PROJ (0, 0)
    ui.navigate2D(0, -1, song);
    REQUIRE(ui.current_view == toad::VIEW_PROJECT);

    // From PROJ, move RIGHT -> SETT (1, 0)
    ui.navigate2D(1, 0, song);
    REQUIRE(ui.current_view == toad::VIEW_SETTINGS);

    // From SETT, move DOWN -> CHAIN (1, 1)
    ui.navigate2D(0, 1, song);
    REQUIRE(ui.current_view == toad::VIEW_CHAIN);

    // From CHAIN, move RIGHT -> PHRASE (2, 1)
    ui.navigate2D(1, 0, song);
    REQUIRE(ui.current_view == toad::VIEW_PHRASE);

    // From PHRASE, move DOWN -> TABLE (2, 2)
    ui.navigate2D(0, 1, song);
    REQUIRE(ui.current_view == toad::VIEW_TABLE);

    // From TABLE, move LEFT -> SYNTH (1, 2)
    ui.navigate2D(-1, 0, song);
    REQUIRE(ui.current_view == toad::VIEW_SYNTH);

    // From SYNTH, move LEFT -> INST (0, 2)
    ui.navigate2D(-1, 0, song);
    REQUIRE(ui.current_view == toad::VIEW_INSTRUMENT);

    // From INST, move UP -> SONG (0, 1)
    ui.navigate2D(0, -1, song);
    REQUIRE(ui.current_view == toad::VIEW_SONG);
}

TEST_CASE("NavigationMap: Boundary Clamping", "[navmap]") {
    toad::UIState ui;
    toad::Song song;

    // Edge clamping at SONG (0, 1): moving Left should stay at SONG
    ui.current_view = toad::VIEW_SONG;
    ui.navigate2D(-1, 0, song);
    REQUIRE(ui.current_view == toad::VIEW_SONG);

    // Edge clamping at PROJ (0, 0): moving Up or Left should stay at PROJ
    ui.current_view = toad::VIEW_PROJECT;
    ui.navigate2D(0, -1, song);
    REQUIRE(ui.current_view == toad::VIEW_PROJECT);
    ui.navigate2D(-1, 0, song);
    REQUIRE(ui.current_view == toad::VIEW_PROJECT);

    // Edge clamping at PHRASE (2, 1): moving Right or Up should stay at PHRASE
    ui.current_view = toad::VIEW_PHRASE;
    ui.navigate2D(1, 0, song);
    REQUIRE(ui.current_view == toad::VIEW_PHRASE);
    ui.navigate2D(0, -1, song);
    REQUIRE(ui.current_view == toad::VIEW_PHRASE);

    // Edge clamping at TABLE (2, 2): moving Down or Right should stay at TABLE
    ui.current_view = toad::VIEW_TABLE;
    ui.navigate2D(0, 1, song);
    REQUIRE(ui.current_view == toad::VIEW_TABLE);
    ui.navigate2D(1, 0, song);
    REQUIRE(ui.current_view == toad::VIEW_TABLE);
}

TEST_CASE("NavigationMap: Contextual Linking on 2D Navigation", "[navmap]") {
    toad::UIState ui;
    toad::Song song;

    // Populate song row 0 track 0 with chain 7
    song.rows[0].chain_ids[0] = 7;
    // Populate chain 7 step 0 with phrase 9
    song.chains[7].steps[0].phrase_id = 9;
    // Populate phrase 9 step 0 with instrument 4
    song.phrases[9].steps[0].instrument = 4;
    // Populate instrument 4 with table 3
    song.instruments[4].table_id = 3;

    ui.current_view = toad::VIEW_SONG;
    ui.cursor_row = 0;
    ui.active_track = 0;

    // Navigate to CHAIN: selected_chain_id should become 7
    ui.navigate2D(1, 0, song);
    REQUIRE(ui.current_view == toad::VIEW_CHAIN);
    REQUIRE(ui.selected_chain_id == 7);

    // Navigate to PHRASE: selected_phrase_id should become 9
    ui.navigate2D(1, 0, song);
    REQUIRE(ui.current_view == toad::VIEW_PHRASE);
    REQUIRE(ui.selected_phrase_id == 9);

    // Navigate to TABLE: selected_table_id should link to table 3
    ui.navigate2D(0, 1, song);
    REQUIRE(ui.current_view == toad::VIEW_TABLE);
    REQUIRE(ui.selected_table_id == 3);
}

TEST_CASE("NavigationMap: HUD Timer and DisplayEngine Rasterization", "[navmap]") {
    toad::UIState ui;
    toad::Song song;
    auto engine = std::make_unique<toad::Engine>();
    auto display = std::make_unique<toad::DisplayEngine>();

    // Initial state
    REQUIRE(ui.nav_hud_timer == 0);

    // Trigger navigation: sets timer to 60
    ui.navigate2D(1, 0, song);
    REQUIRE(ui.nav_hud_timer == 60);

    // Tick HUD timer
    ui.tickNavHUD();
    REQUIRE(ui.nav_hud_timer == 59);

    // Render with HUD active without crashes or allocations
    display->render(ui, song, *engine);
    const uint32_t* argb = display->getArgbBuffer();
    REQUIRE(argb != nullptr);

    // Fast-forward timer to 0
    for (int i = 0; i < 60; ++i) ui.tickNavHUD();
    REQUIRE(ui.nav_hud_timer == 0);

    // Render without HUD overlay
    display->render(ui, song, *engine);
    REQUIRE(argb[0] == toad::DisplayEngine::Colors::BG_OBSIDIAN);
}
