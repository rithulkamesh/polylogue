include(FetchContent)

if(POLYLOGUE_BUILD_HOST)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    # Keep JUCE headers out of our warning set.
    foreach(module IN ITEMS juce_core juce_data_structures juce_events juce_graphics
                            juce_gui_basics juce_gui_extra juce_audio_basics juce_audio_devices
                            juce_audio_formats juce_audio_processors juce_audio_plugin_client
                            juce_audio_utils)
        get_target_property(includes ${module} INTERFACE_INCLUDE_DIRECTORIES)
        set_target_properties(${module} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${includes}")
    endforeach()
endif()

if(POLYLOGUE_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
