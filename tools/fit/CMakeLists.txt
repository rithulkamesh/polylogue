find_package(Threads REQUIRED)
add_executable(polylogue-fit main.cpp)
target_link_libraries(polylogue-fit PRIVATE polylogue_offline polylogue_presets polylogue_options Threads::Threads)
