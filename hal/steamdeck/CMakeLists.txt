# ToadTracker Steam Deck HAL Target
message(STATUS "Configuring ToadTracker Valve Steam Deck (SteamOS) HAL")

add_library(toad_hal_steamdeck STATIC
    steamdeck_harness.h
    steamdeck_harness.cpp
)

target_include_directories(toad_hal_steamdeck PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/hal/common
)

target_link_libraries(toad_hal_steamdeck PUBLIC
    toad_core
)
