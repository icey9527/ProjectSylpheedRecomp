cmake_minimum_required(VERSION 3.25)
project(sylpheed_subtitle_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
find_package(rexglue 0.10.0 EXACT REQUIRED CONFIG)
add_executable(subtitle_settings_test subtitle_settings_test.cpp
    ../../src/patches/movie/development/subtitle_settings.cpp)
target_include_directories(subtitle_settings_test PRIVATE ../.. ../../src)
target_link_libraries(subtitle_settings_test PRIVATE rex::runtime)
rexglue_apply_target_settings(subtitle_settings_test)
if(WIN32)
    add_custom_command(TARGET subtitle_settings_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:subtitle_settings_test>
            $<TARGET_FILE_DIR:subtitle_settings_test> COMMAND_EXPAND_LISTS VERBATIM)
endif()
enable_testing()
add_test(NAME subtitle_settings COMMAND subtitle_settings_test)
