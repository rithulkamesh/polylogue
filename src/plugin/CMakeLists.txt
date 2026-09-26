juce_add_plugin(Polylogue
    PRODUCT_NAME "Polylogue"
    COMPANY_NAME "Polylogue"
    BUNDLE_ID com.polylogue.synth
    PLUGIN_MANUFACTURER_CODE Plgl
    PLUGIN_CODE Plyl
    FORMATS AU VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${POLYLOGUE_COPY_PLUGINS}
)

target_sources(Polylogue PRIVATE PluginEntry.cpp)
set_source_files_properties(PluginEntry.cpp PROPERTIES COMPILE_OPTIONS "${POLYLOGUE_WARNING_FLAGS}")

target_compile_definitions(Polylogue PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=0
)

target_link_libraries(Polylogue
    PRIVATE
        polylogue_ui
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
)
