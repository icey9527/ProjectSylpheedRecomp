# project_sylpheed - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(project_sylpheed LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(PROJECTSYLPHEED_SOURCES
    src/app/main.cpp
    src/audio/host_audio_system.cpp
    src/input/keyboard_keystroke_driver.cpp
    src/patches/input/development/vibration_trace.cpp
    src/patches/movie/development/movie_fallback.cpp
    src/features/performance/frame_metrics.cpp
    src/features/performance/affinity_warning_filter.cpp
    src/features/performance/performance_display.cpp
    src/patches/graphics/development/frame_probe.cpp
)

if(WIN32)
    add_executable(project_sylpheed WIN32 ${PROJECTSYLPHEED_SOURCES})
    target_sources(project_sylpheed PRIVATE src/platform/windows/performance_menu.cpp)
    target_link_libraries(project_sylpheed PRIVATE comctl32 psapi)
else()
    add_executable(project_sylpheed ${PROJECTSYLPHEED_SOURCES})
endif()

# The GPU plugin is loaded at runtime, so request explicit DLL deployment.
rexglue_setup_target(project_sylpheed GPU_PLUGINS xenos)
