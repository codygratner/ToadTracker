add_executable(toad_steamdeck_runner WIN32
    main.cpp
)

target_include_directories(toad_steamdeck_runner PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/hal
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(toad_steamdeck_runner PRIVATE
    toad_core
    toad_hal_steamdeck
    winmm
    xinput
)
