#include <catch2/catch_test_macros.hpp>
#include "toad/keymap.h"
#include "toad/ui_state.h"
#include "toad/display_engine.h"
#include <cstdio>

TEST_CASE("KeyMap: m8.run Preset Key and Gamepad Mappings", "[settings]") {
    toad::InputMap keyMap;
    keyMap.loadM8RunPreset();

    REQUIRE(keyMap.getActivePreset() == toad::PRESET_M8_RUN);

    // Page Navigation via A and S
    REQUIRE(keyMap.resolveKey('A') == toad::INPUT_PAGE_PREV);
    REQUIRE(keyMap.resolveKey('S') == toad::INPUT_PAGE_NEXT);

    // Classic M8 Action buttons Z, X, C, V
    REQUIRE(keyMap.resolveKey('Z') == toad::INPUT_BTN_A);
    REQUIRE(keyMap.resolveKey('X') == toad::INPUT_BTN_B);
    REQUIRE(keyMap.resolveKey('C') == toad::INPUT_BTN_OPT);
    REQUIRE(keyMap.resolveKey('V') == toad::INPUT_BTN_EDIT);

    // Transport via Spacebar
    REQUIRE(keyMap.resolveKey(toad::TOAD_VK_SPACE) == toad::INPUT_TRANSPORT);

    // Gamepad mappings (LB -> Page Prev, RB -> Page Next, A -> Btn A, B -> Btn B)
    REQUIRE(keyMap.resolveGamepad(0x0100 /* LB */) == toad::INPUT_PAGE_PREV);
    REQUIRE(keyMap.resolveGamepad(0x0200 /* RB */) == toad::INPUT_PAGE_NEXT);
    REQUIRE(keyMap.resolveGamepad(0x1000 /* A */) == toad::INPUT_BTN_A);
    REQUIRE(keyMap.resolveGamepad(0x2000 /* B */) == toad::INPUT_BTN_B);
    REQUIRE(keyMap.resolveGamepad(0x4000 /* X */) == toad::INPUT_BTN_OPT);
    REQUIRE(keyMap.resolveGamepad(0x8000 /* Y */) == toad::INPUT_BTN_EDIT);
    REQUIRE(keyMap.resolveGamepad(0x0010 /* Start */) == toad::INPUT_TRANSPORT);
}

TEST_CASE("KeyMap: Presets Desktop, WASD, and Vim", "[settings]") {
    toad::InputMap keyMap;

    // Desktop Tracker Preset
    keyMap.loadDesktopPreset();
    REQUIRE(keyMap.getActivePreset() == toad::PRESET_DESKTOP);
    REQUIRE(keyMap.resolveKey(toad::TOAD_VK_RETURN) == toad::INPUT_BTN_A);
    REQUIRE(keyMap.resolveKey(toad::TOAD_VK_ESCAPE) == toad::INPUT_BTN_B);

    // WASD Preset
    keyMap.loadWasdPreset();
    REQUIRE(keyMap.getActivePreset() == toad::PRESET_WASD);
    REQUIRE(keyMap.resolveKey('W') == toad::INPUT_UP);
    REQUIRE(keyMap.resolveKey('S') == toad::INPUT_DOWN);
    REQUIRE(keyMap.resolveKey('A') == toad::INPUT_LEFT);
    REQUIRE(keyMap.resolveKey('D') == toad::INPUT_RIGHT);
    REQUIRE(keyMap.resolveKey('Q') == toad::INPUT_PAGE_PREV);
    REQUIRE(keyMap.resolveKey('E') == toad::INPUT_PAGE_NEXT);

    // Vim Preset
    keyMap.loadVimPreset();
    REQUIRE(keyMap.getActivePreset() == toad::PRESET_VIM);
    REQUIRE(keyMap.resolveKey('K') == toad::INPUT_UP);
    REQUIRE(keyMap.resolveKey('J') == toad::INPUT_DOWN);
    REQUIRE(keyMap.resolveKey('H') == toad::INPUT_LEFT);
    REQUIRE(keyMap.resolveKey('L') == toad::INPUT_RIGHT);
}

TEST_CASE("KeyMap: Virtual Piano Keyboard Note Resolution", "[settings]") {
    // Base Octave 4 (C-4 = 60)
    // White Keys: Q..]
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('Q', 0) == 60); // C-4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('W', 0) == 62); // D-4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('E', 0) == 64); // E-4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('R', 0) == 65); // F-4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('T', 0) == 67); // G-4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('Y', 0) == 69); // A-4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('U', 0) == 71); // B-4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('I', 0) == 72); // C-5
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('O', 0) == 74); // D-5
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('P', 0) == 76); // E-5
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('[', 0) == 77); // F-5
    REQUIRE(toad::InputMap::resolveVirtualPianoNote(']', 0) == 79); // G-5

    // Black Keys: 2, 3, 5, 6, 7, 9, 0, =
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('2', 0) == 61); // C#4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('3', 0) == 63); // D#4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('5', 0) == 66); // F#4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('6', 0) == 68); // G#4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('7', 0) == 70); // A#4
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('9', 0) == 73); // C#5
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('0', 0) == 75); // D#5
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('=', 0) == 78); // F#5

    // Octave Shift Testing
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('Q', -1) == 48); // C-3
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('Q', +1) == 72); // C-5
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('Q', +2) == 84); // C-6

    // Clamping to MIDI 0..127 range
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('Q', -5) == 0);
    REQUIRE(toad::InputMap::resolveVirtualPianoNote(']', +5) == 127);

    // Non-piano key returns -1
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('Z', 0) == -1);
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('A', 0) == -1);
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('S', 0) == -1);
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('4', 0) == -1);
    REQUIRE(toad::InputMap::resolveVirtualPianoNote('8', 0) == -1);
}

TEST_CASE("KeyMap: Octave Shifting Key Helpers", "[settings]") {
    // Comma / < for Octave Down
    REQUIRE(toad::InputMap::isOctaveDownKey(',') == true);
    REQUIRE(toad::InputMap::isOctaveDownKey(toad::TOAD_VK_OEM_COMMA) == true);
    REQUIRE(toad::InputMap::isOctaveDownKey('.') == false);

    // Period / > for Octave Up
    REQUIRE(toad::InputMap::isOctaveUpKey('.') == true);
    REQUIRE(toad::InputMap::isOctaveUpKey(toad::TOAD_VK_OEM_PER) == true);
    REQUIRE(toad::InputMap::isOctaveUpKey(',') == false);
}

TEST_CASE("KeyMap: Runtime Rebinding and Binary Persistence", "[settings]") {
    toad::InputMap map1;
    map1.loadM8RunPreset();

    // Verify initial binding
    REQUIRE(map1.resolveKey('A') == toad::INPUT_PAGE_PREV);

    // Remap primary key of index 0 (INPUT_PAGE_PREV) to 'P'
    map1.remapPrimaryKey(0, 'P');
    REQUIRE(map1.resolveKey('P') == toad::INPUT_PAGE_PREV);

    // Save to temp binary file
    const char* testFile = "test_keymap_temp.dat";
    REQUIRE(map1.saveToFile(testFile) == true);

    // Load into new instance
    toad::InputMap map2;
    REQUIRE(map2.loadFromFile(testFile) == true);

    // Verify loaded binding matches remapped key
    REQUIRE(map2.resolveKey('P') == toad::INPUT_PAGE_PREV);
    REQUIRE(map2.getActivePreset() == toad::PRESET_M8_RUN);

    // Clean up temporary file
    std::remove(testFile);
}

TEST_CASE("UIState & DisplayEngine: Settings View Navigation and Rendering", "[settings]") {
    toad::UIState ui;
    toad::Song song;
    auto engine = std::make_unique<toad::Engine>();
    auto display = std::make_unique<toad::DisplayEngine>();
    toad::InputMap keyMap;

    // Navigate to SETTINGS view
    ui.current_view = toad::VIEW_SETTINGS;
    REQUIRE(ui.current_view == toad::VIEW_SETTINGS);

    // Page Prev from SETTINGS goes to PROJECT view
    ui.pagePrev();
    REQUIRE(ui.current_view == toad::VIEW_PROJECT);

    // Page Next from PROJECT goes to SETTINGS view
    ui.pageNext(song);
    REQUIRE(ui.current_view == toad::VIEW_SETTINGS);

    // Page Next from SETTINGS wraps to SONG view
    ui.pageNext(song);
    REQUIRE(ui.current_view == toad::VIEW_SONG);

    // Return to SETTINGS
    ui.current_view = toad::VIEW_SETTINGS;

    // Test cursor movement within 18 configurable actions
    ui.moveCursor(5, 0);
    REQUIRE(ui.cursor_row == 5);
    REQUIRE(ui.remap_action_index == 5);

    // Test remap state flag
    ui.is_remapping = true;
    REQUIRE(ui.is_remapping == true);

    // Render with keyMap in normal and remapping state without memory errors
    display->render(ui, song, *engine, keyMap);
    const uint32_t* argb = display->getArgbBuffer();
    REQUIRE(argb != nullptr);

    ui.is_remapping = false;
    display->render(ui, song, *engine, keyMap);
    REQUIRE(argb[0] == toad::DisplayEngine::Colors::BG_OBSIDIAN);
}
