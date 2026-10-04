# ToadTracker ESP32 / dadamachines TBD-16 HAL Target
message(STATUS "Configuring ToadTracker ESP32 (TBD-16 / CTAG TBD) HAL")

add_library(toad_hal_esp32 STATIC
    esp32_harness.h
    esp32_harness.cpp
)

target_include_directories(toad_hal_esp32 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/hal/common
)

target_link_libraries(toad_hal_esp32 PUBLIC
    toad_core
)
