#include "toad/serializer.h"
#include <fstream>
#include <filesystem>
#include <cstring>

namespace toad {

uint32_t Serializer::calculateCrc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

bool Serializer::atomicSave(const std::string& finalPath, const uint8_t* data, size_t size) {
    if (!data || size == 0) return false;

    std::string tmpPath = finalPath + ".tmp";

    // 1. Write payload to temporary file
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        out.flush();
        if (!out.good()) return false;
    }

    // 2. Atomically rename temporary file to destination path
    std::error_code ec;
    std::filesystem::rename(tmpPath, finalPath, ec);
    if (ec) {
        // Fallback for Windows cross-volume or destination collision
        std::filesystem::copy_file(tmpPath, finalPath, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmpPath, ec);
    }

    return !ec;
}

std::vector<uint8_t> Serializer::serializeSongToMemory(const Song& song) {
    AssetHeader header;
    header.magic = MAGIC_SONG;
    header.format_version = FORMAT_VERSION_CURRENT;
    header.payload_size = static_cast<uint32_t>(sizeof(Song));
    header.checksum_crc32 = calculateCrc32(reinterpret_cast<const uint8_t*>(&song), sizeof(Song));

    std::vector<uint8_t> buffer(sizeof(AssetHeader) + sizeof(Song));
    std::memcpy(buffer.data(), &header, sizeof(AssetHeader));
    std::memcpy(buffer.data() + sizeof(AssetHeader), &song, sizeof(Song));
    return buffer;
}

bool Serializer::deserializeSongFromMemory(const uint8_t* data, size_t size, Song& outSong) {
    if (!data || size < sizeof(AssetHeader) + sizeof(Song)) return false;

    AssetHeader header;
    std::memcpy(&header, data, sizeof(AssetHeader));

    if (header.magic != MAGIC_SONG) return false;
    if (header.payload_size != sizeof(Song)) return false;

    const uint8_t* payload = data + sizeof(AssetHeader);
    uint32_t check = calculateCrc32(payload, sizeof(Song));
    if (check != header.checksum_crc32) return false;

    std::memcpy(&outSong, payload, sizeof(Song));
    return true;
}

bool Serializer::saveSong(const std::string& filePath, const Song& song) {
    auto buf = serializeSongToMemory(song);
    return atomicSave(filePath, buf.data(), buf.size());
}

bool Serializer::loadSong(const std::string& filePath, Song& outSong) {
    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;

    std::streamsize fileSize = in.tellg();
    if (fileSize < static_cast<std::streamsize>(sizeof(AssetHeader) + sizeof(Song))) return false;

    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    in.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    if (!in.good()) return false;

    return deserializeSongFromMemory(buffer.data(), buffer.size(), outSong);
}

// --- INDIVIDUAL ASSETS PERSISTENCE ---

bool Serializer::saveInstrument(const std::string& filePath, const Instrument& inst) {
    AssetHeader header;
    header.magic = MAGIC_INSTRUMENT;
    header.payload_size = static_cast<uint32_t>(sizeof(Instrument));
    header.checksum_crc32 = calculateCrc32(reinterpret_cast<const uint8_t*>(&inst), sizeof(Instrument));

    std::vector<uint8_t> buffer(sizeof(AssetHeader) + sizeof(Instrument));
    std::memcpy(buffer.data(), &header, sizeof(AssetHeader));
    std::memcpy(buffer.data() + sizeof(AssetHeader), &inst, sizeof(Instrument));
    return atomicSave(filePath, buffer.data(), buffer.size());
}

bool Serializer::loadInstrument(const std::string& filePath, Instrument& outInst) {
    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;
    std::streamsize size = in.tellg();
    if (size < static_cast<std::streamsize>(sizeof(AssetHeader) + sizeof(Instrument))) return false;

    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(buf.data()), size);

    AssetHeader header;
    std::memcpy(&header, buf.data(), sizeof(AssetHeader));
    if (header.magic != MAGIC_INSTRUMENT || header.payload_size != sizeof(Instrument)) return false;

    const uint8_t* payload = buf.data() + sizeof(AssetHeader);
    if (calculateCrc32(payload, sizeof(Instrument)) != header.checksum_crc32) return false;

    std::memcpy(&outInst, payload, sizeof(Instrument));
    return true;
}

bool Serializer::saveChain(const std::string& filePath, const Chain& chain) {
    AssetHeader header;
    header.magic = MAGIC_CHAIN;
    header.payload_size = static_cast<uint32_t>(sizeof(Chain));
    header.checksum_crc32 = calculateCrc32(reinterpret_cast<const uint8_t*>(&chain), sizeof(Chain));

    std::vector<uint8_t> buffer(sizeof(AssetHeader) + sizeof(Chain));
    std::memcpy(buffer.data(), &header, sizeof(AssetHeader));
    std::memcpy(buffer.data() + sizeof(AssetHeader), &chain, sizeof(Chain));
    return atomicSave(filePath, buffer.data(), buffer.size());
}

bool Serializer::loadChain(const std::string& filePath, Chain& outChain) {
    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;
    std::streamsize size = in.tellg();
    if (size < static_cast<std::streamsize>(sizeof(AssetHeader) + sizeof(Chain))) return false;

    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(buf.data()), size);

    AssetHeader header;
    std::memcpy(&header, buf.data(), sizeof(AssetHeader));
    if (header.magic != MAGIC_CHAIN || header.payload_size != sizeof(Chain)) return false;

    const uint8_t* payload = buf.data() + sizeof(AssetHeader);
    if (calculateCrc32(payload, sizeof(Chain)) != header.checksum_crc32) return false;

    std::memcpy(&outChain, payload, sizeof(Chain));
    return true;
}

bool Serializer::savePhrase(const std::string& filePath, const Phrase& phrase) {
    AssetHeader header;
    header.magic = MAGIC_PHRASE;
    header.payload_size = static_cast<uint32_t>(sizeof(Phrase));
    header.checksum_crc32 = calculateCrc32(reinterpret_cast<const uint8_t*>(&phrase), sizeof(Phrase));

    std::vector<uint8_t> buffer(sizeof(AssetHeader) + sizeof(Phrase));
    std::memcpy(buffer.data(), &header, sizeof(AssetHeader));
    std::memcpy(buffer.data() + sizeof(AssetHeader), &phrase, sizeof(Phrase));
    return atomicSave(filePath, buffer.data(), buffer.size());
}

bool Serializer::loadPhrase(const std::string& filePath, Phrase& outPhrase) {
    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;
    std::streamsize size = in.tellg();
    if (size < static_cast<std::streamsize>(sizeof(AssetHeader) + sizeof(Phrase))) return false;

    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(buf.data()), size);

    AssetHeader header;
    std::memcpy(&header, buf.data(), sizeof(AssetHeader));
    if (header.magic != MAGIC_PHRASE || header.payload_size != sizeof(Phrase)) return false;

    const uint8_t* payload = buf.data() + sizeof(AssetHeader);
    if (calculateCrc32(payload, sizeof(Phrase)) != header.checksum_crc32) return false;

    std::memcpy(&outPhrase, payload, sizeof(Phrase));
    return true;
}

bool Serializer::saveTable(const std::string& filePath, const Table& table) {
    AssetHeader header;
    header.magic = MAGIC_TABLE;
    header.payload_size = static_cast<uint32_t>(sizeof(Table));
    header.checksum_crc32 = calculateCrc32(reinterpret_cast<const uint8_t*>(&table), sizeof(Table));

    std::vector<uint8_t> buffer(sizeof(AssetHeader) + sizeof(Table));
    std::memcpy(buffer.data(), &header, sizeof(AssetHeader));
    std::memcpy(buffer.data() + sizeof(AssetHeader), &table, sizeof(Table));
    return atomicSave(filePath, buffer.data(), buffer.size());
}

bool Serializer::loadTable(const std::string& filePath, Table& outTable) {
    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;
    std::streamsize size = in.tellg();
    if (size < static_cast<std::streamsize>(sizeof(AssetHeader) + sizeof(Table))) return false;

    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(buf.data()), size);

    AssetHeader header;
    std::memcpy(&header, buf.data(), sizeof(AssetHeader));
    if (header.magic != MAGIC_TABLE || header.payload_size != sizeof(Table)) return false;

    const uint8_t* payload = buf.data() + sizeof(AssetHeader);
    if (calculateCrc32(payload, sizeof(Table)) != header.checksum_crc32) return false;

    std::memcpy(&outTable, payload, sizeof(Table));
    return true;
}

} // namespace toad
