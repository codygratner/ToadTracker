cmake_minimum_required(VERSION 3.22)
project(ToadTracker VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

FetchContent_MakeAvailable(Catch2)
if(BUILD_JUCE_TARGET)
    FetchContent_MakeAvailable(JUCE)
endif()

enable_testing()

add_subdirectory(core)
add_subdirectory(test)

if(BUILD_JUCE_TARGET)
    add_subdirectory(hal/juce)
endif()
