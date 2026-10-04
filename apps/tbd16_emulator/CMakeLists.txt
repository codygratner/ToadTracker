add_executable(toad_tbd16_emulator WIN32
    main.cpp
)

target_include_directories(toad_tbd16_emulator PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/hal
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(toad_tbd16_emulator PRIVATE
    toad_core
    toad_hal_esp32
    winmm
    xinput
)
