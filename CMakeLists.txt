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
    src/app/resource_paths.cpp
    src/audio/host_audio_system.cpp
    src/input/keyboard_keystroke_driver.cpp
    src/patches/input/development/vibration_trace.cpp
    src/patches/movie/development/movie_fallback.cpp
    src/patches/movie/development/subtitle_settings.cpp
    src/patches/resources/development/bootstrap.cpp
    src/features/performance/frame_metrics.cpp
    src/features/performance/affinity_warning_filter.cpp
    src/features/performance/performance_display.cpp
    src/patches/graphics/development/frame_probe.cpp
)

if(WIN32)
    add_executable(project_sylpheed WIN32 ${PROJECTSYLPHEED_SOURCES})
    target_sources(project_sylpheed PRIVATE src/platform/windows/tools_menu.cpp
        src/platform/windows/resource_picker.cpp)
    target_link_libraries(project_sylpheed PRIVATE comctl32 psapi ole32 shell32 uuid)
else()
    add_executable(project_sylpheed ${PROJECTSYLPHEED_SOURCES})
endif()

# The GPU plugin is loaded at runtime, so request explicit DLL deployment.
rexglue_setup_target(project_sylpheed GPU_PLUGINS xenos)

# The matching development XEX2 is an external runtime dependency. Keep it
# beside the host executable while leaving the selected game resources under
# game_data_root.
add_custom_command(TARGET project_sylpheed POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/Xacalite_ScriptTeam.exe"
        "$<TARGET_FILE_DIR:project_sylpheed>/BaseLib.dll"
    VERBATIM)
