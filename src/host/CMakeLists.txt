# JUCE-facing code. An INTERFACE library so the JUCE modules it needs are compiled once per
# consuming target (plugin, tests) instead of being duplicated through a static library.
add_library(polylogue_host INTERFACE)

target_sources(polylogue_host INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/MidiMapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MidiTranslator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParameterLayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PresetManager.cpp
)

target_include_directories(polylogue_host INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(polylogue_host INTERFACE
    polylogue_dsp
    polylogue_presets
    juce::juce_audio_processors
    juce::juce_recommended_config_flags
)
