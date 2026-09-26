add_executable(polylogue-screenshot main.cpp)
set_source_files_properties(main.cpp PROPERTIES COMPILE_OPTIONS "${POLYLOGUE_WARNING_FLAGS}")
target_link_libraries(polylogue-screenshot PRIVATE polylogue_ui)
target_compile_definitions(polylogue-screenshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
