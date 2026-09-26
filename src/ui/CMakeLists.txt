# The interface. Like the host layer this is an INTERFACE source library, compiled by whichever
# target consumes it (the plugin, the screenshot tool).
juce_add_binary_data(PolylogueAssets SOURCES
    ${CMAKE_SOURCE_DIR}/assets/fonts/IBMPlexMono-Regular.ttf
    ${CMAKE_SOURCE_DIR}/assets/fonts/IBMPlexSans-Regular.ttf
    ${CMAKE_SOURCE_DIR}/assets/fonts/IBMPlexSans-SemiBold.ttf
)

add_library(polylogue_ui INTERFACE)

target_sources(polylogue_ui INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/ChipGroup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Knob.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MappableControl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/KeyboardBar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lcd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PanelLayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PresetMenu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ScopeView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Theme.cpp
)

target_include_directories(polylogue_ui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(polylogue_ui INTERFACE
    polylogue_host
    PolylogueAssets
    juce::juce_gui_basics
    juce::juce_audio_utils
)
