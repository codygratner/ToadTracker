#include <catch2/catch_test_macros.hpp>
#include "toad/serializer.h"
#include "toad/auto_sampler.h"
#include "toad/format_bridge.h"
#include <filesystem>
#include <cmath>

TEST_CASE("Serializer: In-Memory and Binary File Persistence", "[interchange]") {
    toad::Song originalSong;
    originalSong.bpm = 135.0f;
    originalSong.groove = 8;
    originalSong.rows[0].chain_ids[0] = 7;
    originalSong.chains[7].steps[0].phrase_id = 14;
    originalSong.phrases[14].steps[0].note = 64; // E-4
    originalSong.phrases[14].steps[0].volume = 0xAA;
    originalSong.tables[0].speed = 2;
    originalSong.tables[0].rows[0].transpose = 12;

    // 1. In-Memory Round-Trip
    auto serialized = toad::Serializer::serializeSongToMemory(originalSong);
    REQUIRE(serialized.size() > sizeof(toad::AssetHeader));

    toad::Song loadedFromMemory;
    REQUIRE(toad::Serializer::deserializeSongFromMemory(serialized.data(), serialized.size(), loadedFromMemory));
    REQUIRE(loadedFromMemory.bpm == 135.0f);
    REQUIRE(loadedFromMemory.groove == 8);
    REQUIRE(loadedFromMemory.rows[0].chain_ids[0] == 7);
    REQUIRE(loadedFromMemory.phrases[14].steps[0].note == 64);
    REQUIRE(loadedFromMemory.tables[0].rows[0].transpose == 12);

    // 2. Corrupted Payload Rejection
    auto corrupted = serialized;
    corrupted[sizeof(toad::AssetHeader) + 5] ^= 0xFF; // Invert byte in payload
    toad::Song corruptSong;
    REQUIRE_FALSE(toad::Serializer::deserializeSongFromMemory(corrupted.data(), corrupted.size(), corruptSong));

    // 3. Atomic File Save and Load
    std::string testPath = "test_song.tts";
    REQUIRE(toad::Serializer::saveSong(testPath, originalSong));
    REQUIRE(std::filesystem::exists(testPath));

    toad::Song loadedFromFile;
    REQUIRE(toad::Serializer::loadSong(testPath, loadedFromFile));
    REQUIRE(loadedFromFile.bpm == 135.0f);
    REQUIRE(loadedFromFile.rows[0].chain_ids[0] == 7);
    std::filesystem::remove(testPath);

    // 4. Individual Asset Persistence (Phrase & Table)
    std::string phrasePath = "test_phrase.ttp";
    REQUIRE(toad::Serializer::savePhrase(phrasePath, originalSong.phrases[14]));
    toad::Phrase loadedPhrase;
    REQUIRE(toad::Serializer::loadPhrase(phrasePath, loadedPhrase));
    REQUIRE(loadedPhrase.steps[0].note == 64);
    std::filesystem::remove(phrasePath);

    std::string tablePath = "test_table.ttt";
    REQUIRE(toad::Serializer::saveTable(tablePath, originalSong.tables[0]));
    toad::Table loadedTable;
    REQUIRE(toad::Serializer::loadTable(tablePath, loadedTable));
    REQUIRE(loadedTable.rows[0].transpose == 12);
    std::filesystem::remove(tablePath);
}

TEST_CASE("AutoSampler: Zero-Crossing Loop Detection and WAV Export", "[interchange]") {
    toad::Engine engine;
    toad::Song song;
    // Setup instrument 0 as Alpha Juno PWM Saw
    song.instruments[0].type = toad::INST_TYPE_INTERNAL_SYNTH;
    song.instruments[0].synth.pulse_width = 0.5f;
    song.instruments[0].filter_type = toad::FLT_BYPASS;
    engine.loadSong(song);

    // 1. Zero-Crossing Detector Test on Synthetic Waveform
    std::vector<float> sineWave(2048);
    for (size_t i = 0; i < sineWave.size(); ++i) {
        sineWave[i] = std::sin(2.0f * 3.14159265f * static_cast<float>(i) / 128.0f);
    }
    size_t loopStart = 0, loopEnd = 0;
    bool found = toad::AutoSampler::findZeroCrossingLoop(sineWave.data(), sineWave.size(), 256, loopStart, loopEnd);
    REQUIRE(found);
    REQUIRE(loopStart < loopEnd);

    // 2. Chromatic Note Bouncing
    toad::BakedSample baked = toad::AutoSampler::bakeNote(engine, 0, 60, 0.1f, 0.05f, 44100, true);
    REQUIRE(baked.root_note == 60);
    REQUIRE(baked.sample_rate == 44100);
    REQUIRE(baked.pcm_data.size() > 0);

    // 3. RIFF/WAV Container Generation
    auto wavBytes = toad::AutoSampler::createWavFile(baked);
    REQUIRE(wavBytes.size() > 44);
    REQUIRE(wavBytes[0] == 'R');
    REQUIRE(wavBytes[1] == 'I');
    REQUIRE(wavBytes[2] == 'F');
    REQUIRE(wavBytes[3] == 'F');

    // Verify 'WAVE' chunk header
    REQUIRE(wavBytes[8] == 'W');
    REQUIRE(wavBytes[9] == 'A');
    REQUIRE(wavBytes[10] == 'V');
    REQUIRE(wavBytes[11] == 'E');

    // 4. Full Multi-Sample Bank Baking
    toad::AutoSamplerConfig config;
    config.note_start = 60;
    config.note_end = 72;
    config.semitone_step = 6;
    config.note_duration_sec = 0.05f;
    config.release_sec = 0.02f;
    config.detect_loops = false;

    auto bank = toad::AutoSampler::bakeInstrument(engine, 0, config);
    // Notes 60, 66, 72 -> 3 samples
    REQUIRE(bank.size() == 3);
    REQUIRE(bank[0].root_note == 60);
    REQUIRE(bank[1].root_note == 66);
    REQUIRE(bank[2].root_note == 72);
}

TEST_CASE("FormatBridge: Classic Pattern Slicing, Envelope Tables, and M8 Bridge", "[interchange]") {
    // 1. 64-Row Classic Pattern Slicing
    std::vector<toad::ClassicPatternCell> pattern64(64);
    // Row 0 has C-4 (60)
    pattern64[0].note = 60;
    pattern64[0].instrument = 1;
    // Row 16 (Step 0 of Phrase 1) has D-4 (62)
    pattern64[16].note = 62;
    // Row 32 (Step 0 of Phrase 2) has E-4 (64)
    pattern64[32].note = 64;
    // Row 48 (Step 0 of Phrase 3) has F-4 (65)
    pattern64[48].note = 65;

    toad::Chain chain;
    toad::Phrase phrases[4];
    REQUIRE(toad::FormatBridge::slice64RowPattern(pattern64.data(), 0, chain, phrases));

    REQUIRE(chain.steps[0].phrase_id == 0);
    REQUIRE(chain.steps[1].phrase_id == 1);
    REQUIRE(chain.steps[2].phrase_id == 2);
    REQUIRE(chain.steps[3].phrase_id == 3);

    REQUIRE(phrases[0].steps[0].note == 60);
    REQUIRE(phrases[1].steps[0].note == 62);
    REQUIRE(phrases[2].steps[0].note == 64);
    REQUIRE(phrases[3].steps[0].note == 65);

    // 2. Envelope Translation to 16-Row Primary Table
    toad::FormatBridge::EnvelopePoint points[] = {
        {0.0f, 0.0f}, // Attack start
        {0.2f, 1.0f}, // Attack peak
        {0.6f, 0.5f}, // Decay to sustain
        {1.0f, 0.0f}  // Release end
    };
    toad::Table envTable;
    REQUIRE(toad::FormatBridge::translateEnvelopeToTable(points, 4, true, 8, envTable));
    REQUIRE(envTable.speed == 1);
    REQUIRE(envTable.loop == true);
    // Row 0 starts near 0 volume
    REQUIRE(envTable.rows[0].volume == 0);
    // Peak near row 3
    REQUIRE(envTable.rows[3].volume > 200);
    // Last row has CMD_HOP_ loop jump
    REQUIRE(envTable.rows[15].cmd1 == toad::CMD_HOP_);
    REQUIRE(envTable.rows[15].val1 == 8);

    // 3. Channel Compaction
    toad::Phrase chA, chB, merged;
    chA.steps[0].note = 60;
    chA.steps[4].note = 64;
    chB.steps[2].note = 62;
    chB.steps[6].note = 65;

    REQUIRE(toad::FormatBridge::compactSparseChannels(chA, chB, merged));
    REQUIRE(merged.steps[0].note == 60);
    REQUIRE(merged.steps[2].note == 62);
    REQUIRE(merged.steps[4].note == 64);
    REQUIRE(merged.steps[6].note == 65);

    // Collision should fail compaction
    chB.steps[0].note = 70; // Conflicts with chA.steps[0]
    REQUIRE_FALSE(toad::FormatBridge::compactSparseChannels(chA, chB, merged));

    // 4. Dirtywave M8 Command Mapping
    REQUIRE(toad::FormatBridge::m8CommandToToad(toad::M8_CMD_PWM) == toad::CMD_PWM_);
    REQUIRE(toad::FormatBridge::m8CommandToToad(toad::M8_CMD_CUT) == toad::CMD_FCUT);
    REQUIRE(toad::FormatBridge::m8CommandToToad(toad::M8_CMD_RES) == toad::CMD_FRES);
    REQUIRE(toad::FormatBridge::m8CommandToToad(toad::M8_CMD_HOP) == toad::CMD_HOP_);

    REQUIRE(toad::FormatBridge::toadCommandToM8(toad::CMD_PWM_) == toad::M8_CMD_PWM);
    REQUIRE(toad::FormatBridge::toadCommandToM8(toad::CMD_FCUT) == toad::M8_CMD_CUT);
    REQUIRE(toad::FormatBridge::toadCommandToM8(toad::CMD_FRES) == toad::M8_CMD_RES);
    REQUIRE(toad::FormatBridge::toadCommandToM8(toad::CMD_HOP_) == toad::M8_CMD_HOP);

    // 5. M8 Format Export & Import
    toad::Song m8Song;
    m8Song.bpm = 140.0f;
    m8Song.rows[0].chain_ids[0] = 3;
    std::vector<uint8_t> m8Bytes;
    REQUIRE(toad::FormatBridge::exportToM8Format(m8Song, m8Bytes));
    REQUIRE(m8Bytes.size() > 0);

    toad::Song importedM8;
    REQUIRE(toad::FormatBridge::importFromM8Format(m8Bytes.data(), m8Bytes.size(), importedM8));
    REQUIRE(importedM8.bpm == 140.0f);
    REQUIRE(importedM8.rows[0].chain_ids[0] == 3);
}
