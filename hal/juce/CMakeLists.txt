# ToadTracker Desktop JUCE HAL Target
message(STATUS "Configuring ToadTracker Desktop JUCE HAL")

if(TARGET juce::juce_audio_processors)
    juce_add_plugin(toad_desktop
        COMPANY_NAME "ToadTracker"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        PLUGIN_NAME "ToadTracker"
        FORMATS VST3 Standalone
        PRODUCT_NAME "ToadTracker"
    )

    target_sources(toad_desktop PRIVATE
        ToadAudioProcessor.h
        ToadAudioProcessor.cpp
        ToadEditorComponent.h
        ToadEditorComponent.cpp
        JuceMidiOutputSink.h
    )

    target_include_directories(toad_desktop PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/core/include
        ${CMAKE_SOURCE_DIR}/hal/common
    )

    target_compile_definitions(toad_desktop PRIVATE
        TOAD_ENABLE_JUCE=1
    )

    target_link_libraries(toad_desktop PRIVATE
        toad_core
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    )
endif()
