add_executable(toad_tracker WIN32
    main.cpp
)

target_include_directories(toad_tracker PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/hal
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(toad_tracker PRIVATE
    toad_core
    winmm
    xinput
)
