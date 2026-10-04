# ToadTracker Raspberry Pi HAL Target
message(STATUS "Configuring ToadTracker Raspberry Pi (Linux ARM) HAL")

add_library(toad_hal_rpi STATIC
    rpi_harness.h
    rpi_harness.cpp
)

target_include_directories(toad_hal_rpi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/hal/common
)

target_link_libraries(toad_hal_rpi PUBLIC
    toad_core
)
