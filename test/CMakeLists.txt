add_executable(toad_core_tests
    test_main.cpp
    test_fast_math.cpp
    test_tables.cpp
    test_scale_pool.cpp
    test_sequencer.cpp
    test_synth.cpp
    test_filter.cpp
    test_audio_render.cpp
    test_ui.cpp
    test_hal.cpp
)

target_include_directories(toad_core_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/hal
)

target_link_libraries(toad_core_tests PRIVATE
    toad_core
    toad_hal_steamdeck
    Catch2::Catch2
)

target_compile_features(toad_core_tests PRIVATE cxx_std_20)

include(CTest)
include(Catch)
catch_discover_tests(toad_core_tests)
