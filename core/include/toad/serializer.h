#pragma once

#include "toad/types.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace toad {

// ============================================================================
// ASSET MAGIC IDENTIFIERS (3-Byte ASCII + Null Terminator)
// ============================================================================
constexpr uint32_t MAGIC_SONG       = 0x00535454; // "TTS\0"
constexpr uint32_t MAGIC_INSTRUMENT = 0x00495454; // "TTI\0"
constexpr uint32_t MAGIC_CHAIN      = 0x00435454; // "TTC\0"
constexpr uint32_t MAGIC_PHRASE     = 0x00505454; // "TTP\0"
constexpr uint32_t MAGIC_TABLE      = 0x00545454; // "TTT\0"
constexpr uint32_t MAGIC_WAVETABLE  = 0x00575454; // "TTW\0"
constexpr uint32_t MAGIC_ZONE       = 0x005A5454; // "TTZ\0"

constexpr uint16_t FORMAT_VERSION_CURRENT = 0x0100; // v1.0

// ============================================================================
// ASSET FILE HEADER
// ============================================================================
#pragma pack(push, 1)
struct AssetHeader {
    uint32_t magic{0};
    uint16_t format_version{FORMAT_VERSION_CURRENT};
    uint16_t flags{0};
    uint32_t payload_size{0};
    uint32_t checksum_crc32{0};
};
#pragma pack(pop)

// ============================================================================
// SERIALIZER & ATOMIC PERSISTENCE ENGINE
// ============================================================================
class Serializer {
public:
    // CRC32 verification (IEEE 802.3 standard)
    static uint32_t calculateCrc32(const uint8_t* data, size_t size);

    // Atomic Save Pattern (Section 9.2)
    // Writes to path.tmp, syncs, then atomically renames to final path
    static bool atomicSave(const std::string& finalPath, const uint8_t* data, size_t size);

    // Master Song (.tts) Persistence
    static bool saveSong(const std::string& filePath, const Song& song);
    static bool loadSong(const std::string& filePath, Song& outSong);

    // Individual Asset Persistence
    static bool saveInstrument(const std::string& filePath, const Instrument& inst);
    static bool loadInstrument(const std::string& filePath, Instrument& outInst);

    static bool saveChain(const std::string& filePath, const Chain& chain);
    static bool loadChain(const std::string& filePath, Chain& outChain);

    static bool savePhrase(const std::string& filePath, const Phrase& phrase);
    static bool loadPhrase(const std::string& filePath, Phrase& outPhrase);

    static bool saveTable(const std::string& filePath, const Table& table);
    static bool loadTable(const std::string& filePath, Table& outTable);

    // In-Memory Binary Serialization
    static std::vector<uint8_t> serializeSongToMemory(const Song& song);
    static bool deserializeSongFromMemory(const uint8_t* data, size_t size, Song& outSong);
};

} // namespace toad
