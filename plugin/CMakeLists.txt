add_library(dg_plugin_shared INTERFACE)
target_sources(dg_plugin_shared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/ParameterLayout.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ParameterLayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp)
target_include_directories(dg_plugin_shared INTERFACE ${PROJECT_SOURCE_DIR})
target_compile_definitions(dg_plugin_shared INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(dg_plugin_shared INTERFACE
    dg_engine
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

juce_add_plugin(Dubgefahren
    COMPANY_NAME "atze187"
    BUNDLE_ID "de.atze187.dubgefahren"
    PLUGIN_MANUFACTURER_CODE Atze
    PLUGIN_CODE Dgfn
    FORMATS VST3
    PRODUCT_NAME "Dubgefahren"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)
target_link_libraries(Dubgefahren PRIVATE dg_plugin_shared)
