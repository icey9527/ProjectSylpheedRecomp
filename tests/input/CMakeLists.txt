cmake_minimum_required(VERSION 3.25)
project(sylpheed_input_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(rexglue 0.10.0 EXACT REQUIRED CONFIG)
add_executable(keyboard_input_test
    keyboard_input_test.cpp
    ../../src/input/keyboard_keystroke_driver.cpp)
target_include_directories(keyboard_input_test PRIVATE ../../src)
target_link_libraries(keyboard_input_test PRIVATE rex::runtime)
rexglue_apply_target_settings(keyboard_input_test)
if(WIN32)
    add_custom_command(TARGET keyboard_input_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:keyboard_input_test>
            $<TARGET_FILE_DIR:keyboard_input_test>
        COMMAND_EXPAND_LISTS VERBATIM)
endif()
enable_testing()
add_test(NAME keyboard_input COMMAND keyboard_input_test)
