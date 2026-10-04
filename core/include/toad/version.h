#pragma once

#include <cstdint>
#include <string_view>

namespace toad {

constexpr uint32_t VERSION_MAJOR = 0;
constexpr uint32_t VERSION_MINOR = 2;
constexpr uint32_t VERSION_PATCH = 0;

constexpr std::string_view VERSION_STRING = "0.2.0";
constexpr std::string_view APP_NAME = "ToadTracker";
constexpr std::string_view CODENAME = "The Toad";

inline constexpr uint32_t makeVersion(uint32_t major, uint32_t minor, uint32_t patch) {
    return (major << 16) | (minor << 8) | patch;
}

inline constexpr uint32_t getVersionNumber() {
    return makeVersion(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
}

inline constexpr std::string_view getVersionString() {
    return VERSION_STRING;
}

} // namespace toad
