include(Catch)

# ---- DSP tests: pure C++, no JUCE ---------------------------------------------------------------

add_executable(polylogue_dsp_tests
    support/AllocationGuard.cpp
    dsp/AllocationTests.cpp
    dsp/BlepTests.cpp
    dsp/DriveTests.cpp
    dsp/EngineTests.cpp
    dsp/EnvelopeTests.cpp
    dsp/FactoryPresetTests.cpp
    dsp/FilterTests.cpp
    dsp/FmAndChorusTests.cpp
    dsp/GlideTests.cpp
    dsp/LfoTests.cpp
    dsp/ModMatrixTests.cpp
    dsp/MonoAndGlideTests.cpp
    dsp/OscillatorTests.cpp
    dsp/ParameterTests.cpp
    dsp/RobustnessTests.cpp
    dsp/TuningTests.cpp
    dsp/VoiceManagerTests.cpp
    dsp/VoiceTests.cpp
)
target_include_directories(polylogue_dsp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(polylogue_dsp_tests PRIVATE polylogue_offline polylogue_presets
                                                  polylogue_options Catch2::Catch2WithMain)
catch_discover_tests(polylogue_dsp_tests TEST_PREFIX "dsp: ")

# ---- Host tests: the JUCE layer, run headless ----------------------------------------------------

if(POLYLOGUE_BUILD_HOST)
    set(host_test_sources
        support/AllocationGuard.cpp
        host/JuceListener.cpp
        host/EditorTests.cpp
        host/MidiMapperTests.cpp
        host/MidiTranslatorTests.cpp
        host/PresetManagerTests.cpp
        host/ProcessorTests.cpp
        host/ThreadingTests.cpp
    )
    add_executable(polylogue_host_tests ${host_test_sources})
    set_source_files_properties(${host_test_sources} PROPERTIES
        COMPILE_OPTIONS "${POLYLOGUE_WARNING_FLAGS}")
    target_include_directories(polylogue_host_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(polylogue_host_tests PRIVATE polylogue_ui Catch2::Catch2WithMain)
    target_compile_definitions(polylogue_host_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    catch_discover_tests(polylogue_host_tests TEST_PREFIX "host: ")
endif()
