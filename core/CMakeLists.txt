add_library(toad_core INTERFACE)

target_include_directories(toad_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(toad_core INTERFACE cxx_std_20)
