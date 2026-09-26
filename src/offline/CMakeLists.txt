add_library(polylogue_offline STATIC
    OfflineRenderer.cpp
    WavWriter.cpp
)
target_include_directories(polylogue_offline PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(polylogue_offline PUBLIC polylogue_dsp PRIVATE polylogue_options)
