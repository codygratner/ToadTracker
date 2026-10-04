add_executable(toad_core_tests
    test_main.cpp
    test_fast_math.cpp
    test_tables.cpp
    test_scale_pool.cpp
)

target_link_libraries(toad_core_tests PRIVATE
    toad_core
    Catch2::Catch2
)

target_compile_features(toad_core_tests PRIVATE cxx_std_20)

include(CTest)
include(Catch)
catch_discover_tests(toad_core_tests)
