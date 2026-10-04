add_library(toad_core STATIC
    src/engine.cpp
)

target_include_directories(toad_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(toad_core PUBLIC cxx_std_20)
