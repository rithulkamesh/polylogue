add_library(polylogue_dsp STATIC
    Axes.cpp
    Chorus.cpp
    Drive.cpp
    Engine.cpp
    Envelope.cpp
    Fft.cpp
    Filter.cpp
    Lfo.cpp
    ModMatrix.cpp
    Oscillator.cpp
    Parameters.cpp
    Voice.cpp
    VoiceManager.cpp
)
target_include_directories(polylogue_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(polylogue_dsp PUBLIC cxx_std_20)
target_link_libraries(polylogue_dsp PRIVATE polylogue_options)
