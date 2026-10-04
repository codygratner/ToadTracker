#include <catch2/catch_test_macros.hpp>
#include "toad/ui_state.h"
#include "toad/display_engine.h"

TEST_CASE("UIState: Lifecycle and View Navigation Hierarchy", "[ui]") {
    toad::UIState ui;
    toad::Song song;
    toad::Engine engine;
    engine.loadSong(song);

    // Initial state should be VIEW_SONG
    REQUIRE(ui.current_view == toad::VIEW_SONG);
    REQUIRE(ui.cursor_row == 0);
    REQUIRE(ui.cursor_col == 0);

    // Song row 0 track 0 has chain 5
    song.rows[0].chain_ids[0] = 5;
    // Chain 5 step 0 has phrase 12
    song.chains[5].steps[0].phrase_id = 12;

    // Page Next from SONG view enters CHAIN view for chain 5
    ui.pageNext(song);
    REQUIRE(ui.current_view == toad::VIEW_CHAIN);
    REQUIRE(ui.selected_chain_id == 5);

    // Page Next from CHAIN view enters PHRASE view for phrase 12
    ui.pageNext(song);
    REQUIRE(ui.current_view == toad::VIEW_PHRASE);
    REQUIRE(ui.selected_phrase_id == 12);

    // Page Next from PHRASE view enters INSTRUMENT view
    ui.pageNext(song);
    REQUIRE(ui.current_view == toad::VIEW_INSTRUMENT);

    // Page Prev steps back up the hierarchy
    ui.pagePrev();
    REQUIRE(ui.current_view == toad::VIEW_PHRASE);

    ui.pagePrev();
    REQUIRE(ui.current_view == toad::VIEW_CHAIN);

    ui.pagePrev();
    REQUIRE(ui.current_view == toad::VIEW_SONG);
}

TEST_CASE("UIState: Fast Modifier View Toggles (VIEW_MOD & SUB_MOD)", "[ui]") {
    toad::UIState ui;

    // L2 Trigger: Rapid toggle SONG <-> PHRASE
    REQUIRE(ui.current_view == toad::VIEW_SONG);
    ui.toggleViewMod();
    REQUIRE(ui.current_view == toad::VIEW_PHRASE);
    ui.toggleViewMod();
    REQUIRE(ui.current_view == toad::VIEW_SONG);

    // R2 Trigger: Quick cycle SYNTH <-> TABLE <-> INSTRUMENT
    ui.toggleSubMod();
    REQUIRE(ui.current_view == toad::VIEW_SYNTH);
    ui.toggleSubMod();
    REQUIRE(ui.current_view == toad::VIEW_TABLE);
    ui.toggleSubMod();
    REQUIRE(ui.current_view == toad::VIEW_INSTRUMENT);
    ui.toggleSubMod();
    REQUIRE(ui.current_view == toad::VIEW_SYNTH);
}

TEST_CASE("UIState: Cursor Navigation and Clamping", "[ui]") {
    toad::UIState ui;

    // In SONG view: row 0..255, col 0..7
    ui.moveCursor(10, 5);
    REQUIRE(ui.cursor_row == 10);
    REQUIRE(ui.cursor_col == 5);
    REQUIRE(ui.active_track == 5);

    // Test upper clamping
    ui.moveCursor(300, 10);
    REQUIRE(ui.cursor_row == 255);
    REQUIRE(ui.cursor_col == 7);
    REQUIRE(ui.active_track == 7);

    // Test lower clamping
    ui.moveCursor(-500, -20);
    REQUIRE(ui.cursor_row == 0);
    REQUIRE(ui.cursor_col == 0);
    REQUIRE(ui.active_track == 0);
}

TEST_CASE("UIState: Value Editing and Track Controls", "[ui]") {
    toad::UIState ui;
    toad::Song song;
    toad::Engine engine;
    engine.loadSong(song);

    // In Song view, empty cell edited with +1 becomes Chain 0
    REQUIRE(song.rows[0].chain_ids[0] == toad::CHAIN_EMPTY);
    ui.editValue(song, 1, false);
    REQUIRE(song.rows[0].chain_ids[0] == 0);

    // Increment again
    ui.editValue(song, 1, false);
    REQUIRE(song.rows[0].chain_ids[0] == 1);

    // Decrement back down
    ui.editValue(song, -1, false);
    REQUIRE(song.rows[0].chain_ids[0] == 0);
    ui.editValue(song, -1, false);
    REQUIRE(song.rows[0].chain_ids[0] == toad::CHAIN_EMPTY);

    // Switch to PHRASE view and test Note editing
    ui.current_view = toad::VIEW_PHRASE;
    ui.cursor_row = 0;
    ui.cursor_col = 0; // Note column

    REQUIRE(song.phrases[0].steps[0].note == toad::NOTE_EMPTY);
    ui.editValue(song, 1, false); // Triggers C-4 (60)
    REQUIRE(song.phrases[0].steps[0].note == 60);

    // Fine increment (+1 semitone)
    ui.editValue(song, 1, false);
    REQUIRE(song.phrases[0].steps[0].note == 61);

    // Coarse increment (+12 semitones / 1 octave)
    ui.editValue(song, 1, true);
    REQUIRE(song.phrases[0].steps[0].note == 73);

    // Extended Handheld Controls (Grip Buttons)
    toad::InputEvent evOctUp{toad::INPUT_OCTAVE_UP, true};
    ui.handleInput(evOctUp, song, engine);
    REQUIRE(ui.octave_offset == 1);

    toad::InputEvent evOctDown{toad::INPUT_OCTAVE_DOWN, true};
    ui.handleInput(evOctDown, song, engine);
    REQUIRE(ui.octave_offset == 0);

    toad::InputEvent evMute{toad::INPUT_TRACK_MUTE, true};
    ui.handleInput(evMute, song, engine);
    REQUIRE(ui.track_muted[ui.active_track] == true);
    ui.handleInput(evMute, song, engine);
    REQUIRE(ui.track_muted[ui.active_track] == false);
}

TEST_CASE("DisplayEngine: Pixel, Glyph, and View Rasterization", "[ui]") {
    toad::DisplayEngine display;
    toad::UIState ui;
    toad::Song song;
    toad::Engine engine;
    engine.loadSong(song);

    // Verify dimensions
    REQUIRE(toad::DisplayEngine::SCREEN_WIDTH == 240);
    REQUIRE(toad::DisplayEngine::SCREEN_HEIGHT == 240);
    REQUIRE(toad::DisplayEngine::TOTAL_PIXELS == 57600);

    // Clear and verify buffer
    display.clear(toad::DisplayEngine::Colors::BG_OBSIDIAN);
    const uint32_t* argb = display.getArgbBuffer();
    REQUIRE(argb[0] == toad::DisplayEngine::Colors::BG_OBSIDIAN);

    const uint16_t* rgb565 = display.getRgb565Buffer();
    REQUIRE(rgb565 != nullptr);

    // Draw Primitives
    display.drawPixel(10, 10, toad::DisplayEngine::Colors::TEXT_BRIGHT);
    REQUIRE(argb[10 * 240 + 10] == toad::DisplayEngine::Colors::TEXT_BRIGHT);

    display.fillRect(20, 20, 10, 10, toad::DisplayEngine::Colors::TEXT_ACCENT);
    REQUIRE(argb[20 * 240 + 20] == toad::DisplayEngine::Colors::TEXT_ACCENT);

    // Draw Text / Hex / Note
    display.drawString(0, 0, "TOAD", toad::DisplayEngine::Colors::TEXT_BRIGHT);
    display.drawHexByte(30, 0, 0xFE, toad::DisplayEngine::Colors::TEXT_ACCENT);
    display.drawNote(50, 0, 60, toad::DisplayEngine::Colors::TEXT_BRIGHT); // C-4

    // Render Full Frame across all views without errors
    ui.current_view = toad::VIEW_SONG;
    display.render(ui, song, engine);

    ui.current_view = toad::VIEW_CHAIN;
    display.render(ui, song, engine);

    ui.current_view = toad::VIEW_PHRASE;
    display.render(ui, song, engine);

    ui.current_view = toad::VIEW_TABLE;
    display.render(ui, song, engine);

    ui.current_view = toad::VIEW_SYNTH;
    display.render(ui, song, engine);

    // Test Steam Deck 1280x800 3x scaling compositor
    std::vector<uint32_t> steamDeckBuffer(toad::DisplayEngine::STEAM_DECK_WIDTH * toad::DisplayEngine::STEAM_DECK_HEIGHT, 0);
    display.compositeSteamDeck(steamDeckBuffer.data(), ui, engine);

    // Verify center pixel is rendered inside the 720x720 scaled window
    size_t centerIdx = (toad::DisplayEngine::STEAM_DECK_HEIGHT / 2) * toad::DisplayEngine::STEAM_DECK_WIDTH + (toad::DisplayEngine::STEAM_DECK_WIDTH / 2);
    REQUIRE(steamDeckBuffer[centerIdx] != 0);
}
