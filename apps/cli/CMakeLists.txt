add_executable(toad_cli
    main.cpp
)

target_link_libraries(toad_cli PRIVATE
    toad_core
)

target_compile_features(toad_cli PRIVATE cxx_std_20)
