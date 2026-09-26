add_library(polylogue_presets STATIC
    FactoryPresets.cpp
)
target_include_directories(polylogue_presets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(polylogue_presets PUBLIC polylogue_dsp PRIVATE polylogue_options)
