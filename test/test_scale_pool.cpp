#include <catch2/catch_test_macros.hpp>
#include <toad/types.h>
#include <toad/scale.h>
#include <toad/pool.h>

TEST_CASE("Scale: Quantization Engine", "[scale]") {
    toad::ScaleDefinition cMajor;
    cMajor.root_note = 0; // C
    // C Major: C(0), D(2), E(4), F(5), G(7), A(9), B(11)
    // Mask: bit 0, 2, 4, 5, 7, 9, 11 = 1 + 4 + 16 + 32 + 128 + 512 + 2048 = 2741 = 0x0AB5
    cMajor.note_mask = 0x0AB5;
    cMajor.snap_mode = toad::SNAP_NEAREST;
    cMajor.enabled = true;

    SECTION("In-scale notes pass through unchanged") {
        REQUIRE(toad::quantizeToScale(cMajor, 0) == 0);   // C
        REQUIRE(toad::quantizeToScale(cMajor, 2) == 2);   // D
        REQUIRE(toad::quantizeToScale(cMajor, 4) == 4);   // E
        REQUIRE(toad::quantizeToScale(cMajor, 5) == 5);   // F
        REQUIRE(toad::quantizeToScale(cMajor, 7) == 7);   // G
        REQUIRE(toad::quantizeToScale(cMajor, 9) == 9);   // A
        REQUIRE(toad::quantizeToScale(cMajor, 11) == 11); // B
        REQUIRE(toad::quantizeToScale(cMajor, 12) == 12); // C+1
    }

    SECTION("Out-of-scale notes snap down") {
        cMajor.snap_mode = toad::SNAP_DOWN;
        // C# (1) -> C (0)
        REQUIRE(toad::quantizeToScale(cMajor, 1) == 0);
        // D# (3) -> D (2)
        REQUIRE(toad::quantizeToScale(cMajor, 3) == 2);
        // F# (6) -> F (5)
        REQUIRE(toad::quantizeToScale(cMajor, 6) == 5);
    }

    SECTION("Out-of-scale notes snap up") {
        cMajor.snap_mode = toad::SNAP_UP;
        // C# (1) -> D (2)
        REQUIRE(toad::quantizeToScale(cMajor, 1) == 2);
        // D# (3) -> E (4)
        REQUIRE(toad::quantizeToScale(cMajor, 3) == 4);
        // F# (6) -> G (7)
        REQUIRE(toad::quantizeToScale(cMajor, 6) == 7);
    }

    SECTION("Disabled scale does not alter notes") {
        cMajor.enabled = false;
        REQUIRE(toad::quantizeToScale(cMajor, 1) == 1);
        REQUIRE(toad::quantizeToScale(cMajor, 3) == 3);
        REQUIRE(toad::quantizeToScale(cMajor, 6) == 6);
    }
}

TEST_CASE("NotePool: Probabilistic Note Pool Evaluation", "[pool]") {
    toad::InstrumentNotePool pool;
    pool.mode = toad::POOL_OFF;
    pool.slot_count = 2;
    pool.slots[0] = { .semitone_offset = 3, .weight = 0xFF, .velocity_scale = 0xFF };
    pool.slots[1] = { .semitone_offset = 7, .weight = 0xFF, .velocity_scale = 0xFF };

    SECTION("POOL_OFF returns base note without offset") {
        int8_t note = toad::evaluateNotePool(pool, 60);
        REQUIRE(note == 60);
    }

    SECTION("POOL_CYCLE cycles sequentially through active slots") {
        pool.mode = toad::POOL_CYCLE;
        pool.last_selected = 1; // Starting before slot 0

        int8_t note1 = toad::evaluateNotePool(pool, 60);
        REQUIRE(note1 == 63); // 60 + 3
        REQUIRE(pool.last_selected == 0);

        int8_t note2 = toad::evaluateNotePool(pool, 60);
        REQUIRE(note2 == 67); // 60 + 7
        REQUIRE(pool.last_selected == 1);

        int8_t note3 = toad::evaluateNotePool(pool, 60);
        REQUIRE(note3 == 63); // Cycles back to 60 + 3
        REQUIRE(pool.last_selected == 0);
    }

    SECTION("POOL_WEIGHTED_RND selects slot based on weights") {
        pool.mode = toad::POOL_WEIGHTED_RND;
        pool.slots[0].weight = 0;    // 0% chance for offset 3
        pool.slots[1].weight = 0xFF; // 100% chance for offset 7

        for (int i = 0; i < 5; ++i) {
            int8_t note = toad::evaluateNotePool(pool, 60);
            REQUIRE(note == 67); // Only slot 1 can be picked
        }
    }
}
