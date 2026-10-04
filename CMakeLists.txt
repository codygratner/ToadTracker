cmake_minimum_required(VERSION 3.22)
project(ToadTracker VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Architecture & Cross-Compilation Support (ARM / Raspberry Pi / Steam Deck / Desktop)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Targeting ARM: enabling NEON SIMD vectorization")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
            add_compile_options(-march=armv8-a+simd)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
            add_compile_options(-mfpu=neon -mfloat-abi=hard)
        endif()
    endif()
endif()

include(FetchContent)

# 1. Catch2 for Headless Core Tests
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)

# 2. JUCE Framework (GPLv3)
option(BUILD_JUCE_TARGET "Build Desktop Standalone and VST3 Plugin" OFF)
if(BUILD_JUCE_TARGET)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
    )
endif()

# 3. libopenmpt (For Classic Tracker Format Import)
option(ENABLE_MODULE_IMPORT "Enable MOD/XM/IT/S3M Import Engine" OFF)
if(ENABLE_MODULE_IMPORT)
    FetchContent_Declare(
        libopenmpt
        GIT_REPOSITORY https://github.com/OpenMPT/openmpt.git
        GIT_TAG        libopenmpt-0.7.8
    )
endif()

# 4. Target Platforms
option(BUILD_RPI_TARGET "Build Raspberry Pi Linux ARM target (ALSA/DRM/SDL)" ON)
option(BUILD_STEAMDECK_TARGET "Build Valve Steam Deck target (SteamOS/SDL/PipeWire)" ON)
option(BUILD_ESP32_TARGET "Build dadamachines TBD-16 ESP32 target" ON)

FetchContent_MakeAvailable(Catch2)
if(BUILD_JUCE_TARGET)
    FetchContent_MakeAvailable(JUCE)
endif()

enable_testing()

add_subdirectory(core)

if(BUILD_RPI_TARGET)
    add_subdirectory(hal/rpi)
endif()

if(BUILD_STEAMDECK_TARGET)
    add_subdirectory(hal/steamdeck)
endif()

if(BUILD_ESP32_TARGET)
    add_subdirectory(hal/esp32)
endif()

if(BUILD_JUCE_TARGET)
    add_subdirectory(hal/juce)
endif()

add_subdirectory(apps/cli)
add_subdirectory(test)
