# Warnings and sanitizers for our own code.
#
# JUCE compiles its module sources into whichever target links them, so -Werror must never be
# attached to a target that links JUCE. Pure C++ targets link `polylogue_options`; JUCE-facing
# sources get the same flags through polylogue_strict_sources() instead.
if(MSVC)
    set(POLYLOGUE_WARNING_FLAGS /W4 /WX /permissive- /utf-8)
else()
    set(POLYLOGUE_WARNING_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
        -Wimplicit-fallthrough
        -Werror
    )
endif()

add_library(polylogue_options INTERFACE)
target_compile_features(polylogue_options INTERFACE cxx_std_20)
target_compile_options(polylogue_options INTERFACE ${POLYLOGUE_WARNING_FLAGS})

if(POLYLOGUE_SANITIZE AND POLYLOGUE_TSAN)
    message(FATAL_ERROR "POLYLOGUE_SANITIZE and POLYLOGUE_TSAN cannot be combined")
endif()

if(MSVC AND (POLYLOGUE_SANITIZE OR POLYLOGUE_TSAN))
    message(FATAL_ERROR "The sanitizer options need GCC or Clang")
elseif(POLYLOGUE_SANITIZE)
    set(_sanitizers -fsanitize=address,undefined -fno-omit-frame-pointer
                    -fno-sanitize-recover=undefined)
elseif(POLYLOGUE_TSAN)
    set(_sanitizers -fsanitize=thread -fno-omit-frame-pointer)
endif()
if(_sanitizers)
    add_compile_options(${_sanitizers})
    add_link_options(${_sanitizers})
endif()

# Applies the warning flags to the sources of an INTERFACE source library as compiled by the
# targets in `consumer_dirs`. Call after those directories have been added.
function(polylogue_strict_sources library)
    get_target_property(sources ${library} INTERFACE_SOURCES)
    list(FILTER sources INCLUDE REGEX "\\.cpp$")
    foreach(dir IN LISTS ARGN)
        set_source_files_properties(${sources}
            DIRECTORY ${dir}
            PROPERTIES COMPILE_OPTIONS "${POLYLOGUE_WARNING_FLAGS}")
    endforeach()
endfunction()
