#include <catch2/catch_test_macros.hpp>
#include <toad/types.h>

TEST_CASE("TablePlayer: Lifecycle and State Management", "[tables]") {
    toad::TablePlayer player;
    REQUIRE(!player.active);
    REQUIRE(player.current_row == -1);

    SECTION("Trigger activates player at row 0") {
        player.trigger(0x05, 2);
        REQUIRE(player.active);
        REQUIRE(player.table_id == 0x05);
        REQUIRE(player.current_row == 0);
        REQUIRE(player.tick_counter == 0);
        REQUIRE(player.speed == 2);
    }

    SECTION("Stop deactivates player and resets row") {
        player.trigger(0x01, 1);
        player.stop();
        REQUIRE(!player.active);
        REQUIRE(player.current_row == -1);
    }
}

TEST_CASE("TablePlayer: Linear Non-Looping Execution", "[tables]") {
    toad::Table table;
    table.speed = 1;
    table.loop = false;

    toad::TablePlayer player;
    player.trigger(0, 1);

    // Initial state is row 0
    REQUIRE(player.current_row == 0);

    // Advance through rows 1 to 15
    for (int expected_row = 1; expected_row < static_cast<int>(toad::TABLE_ROWS); ++expected_row) {
        bool advanced = player.tick(table);
        REQUIRE(advanced);
        REQUIRE(player.current_row == expected_row);
        REQUIRE(player.active);
    }

    // At row 15, advancing past the end with loop = false should stop
    bool advanced = player.tick(table);
    REQUIRE(!advanced);
    REQUIRE(!player.active);
    REQUIRE(player.current_row == -1);
}

TEST_CASE("TablePlayer: Looping Table Execution", "[tables]") {
    toad::Table table;
    table.speed = 1;
    table.loop = true;

    toad::TablePlayer player;
    player.trigger(0, 1);

    // Advance through all 15 remaining rows to reach row 15
    for (int i = 0; i < 15; ++i) {
        player.tick(table);
    }
    REQUIRE(player.current_row == 15);

    // Next tick should wrap around to row 0
    bool wrapped = player.tick(table);
    REQUIRE(wrapped);
    REQUIRE(player.active);
    REQUIRE(player.current_row == 0);

    // Next tick should advance to row 1
    player.tick(table);
    REQUIRE(player.current_row == 1);
}

TEST_CASE("TablePlayer: Multi-Tick Speed Subdivision", "[tables]") {
    toad::Table table;
    table.speed = 4; // 4 ticks per row
    table.loop = false;

    toad::TablePlayer player;
    player.trigger(0, 4);

    REQUIRE(player.current_row == 0);

    // Ticks 1..3 should not advance row
    for (int t = 1; t <= 3; ++t) {
        bool advanced = player.tick(table);
        REQUIRE(!advanced);
        REQUIRE(player.current_row == 0);
        REQUIRE(player.tick_counter == t);
    }

    // Tick 4 should advance to row 1
    bool advanced = player.tick(table);
    REQUIRE(advanced);
    REQUIRE(player.current_row == 1);
    REQUIRE(player.tick_counter == 0);
}

TEST_CASE("TablePlayer: CMD_HOP_ Loop and Jump Execution", "[tables]") {
    toad::Table table;
    table.speed = 1;
    table.loop = false;

    // Set row 3 to hop back to row 1
    table.rows[3].cmd1 = toad::CMD_HOP_;
    table.rows[3].val1 = 0x01;

    toad::TablePlayer player;
    player.trigger(0, 1);

    REQUIRE(player.current_row == 0);
    player.tick(table); REQUIRE(player.current_row == 1);
    player.tick(table); REQUIRE(player.current_row == 2);
    player.tick(table); REQUIRE(player.current_row == 3);

    // Row 3 executes HOP to 1
    player.tick(table);
    REQUIRE(player.current_row == 1);

    // Cycles through 2 -> 3 -> HOP 1 again
    player.tick(table); REQUIRE(player.current_row == 2);
    player.tick(table); REQUIRE(player.current_row == 3);
    player.tick(table); REQUIRE(player.current_row == 1);

    SECTION("CMD_HOP_ on second effect column cmd2") {
        toad::Table table2;
        table2.speed = 1;
        table2.loop = false;
        table2.rows[2].cmd2 = toad::CMD_HOP_;
        table2.rows[2].val2 = 0x00; // Hop to start

        player.trigger(1, 1);
        player.tick(table2); REQUIRE(player.current_row == 1);
        player.tick(table2); REQUIRE(player.current_row == 2);
        player.tick(table2); REQUIRE(player.current_row == 0); // Jumped to 0
    }
}

TEST_CASE("Table: Data Structure Strict Hex and Size Verification", "[tables]") {
    toad::Table table;
    REQUIRE(sizeof(table.rows) / sizeof(toad::TableRow) == toad::TABLE_ROWS);

    // Test signed transpose range (-80..7F)
    table.rows[0].transpose = -128;
    table.rows[1].transpose = 127;
    REQUIRE(table.rows[0].transpose == -128);
    REQUIRE(table.rows[1].transpose == 127);

    // Test volume 00..FF
    table.rows[0].volume = 0x00;
    table.rows[1].volume = 0xFF;
    REQUIRE(table.rows[0].volume == 0x00);
    REQUIRE(table.rows[1].volume == 0xFF);
}
