cmake_minimum_required(VERSION 3.25)
project(sylpheed_audio_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
find_package(rexglue 0.10.0 EXACT REQUIRED CONFIG)
add_executable(audio_queue_test audio_queue_test.cpp ../../src/audio/host_audio_system.cpp)
target_include_directories(audio_queue_test PRIVATE ../../src)
target_link_libraries(audio_queue_test PRIVATE rex::runtime)
rexglue_apply_target_settings(audio_queue_test)
if(WIN32)
    add_custom_command(TARGET audio_queue_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:audio_queue_test>
            $<TARGET_FILE_DIR:audio_queue_test>
        COMMAND_EXPAND_LISTS VERBATIM)
endif()
enable_testing()
add_test(NAME audio_queue COMMAND audio_queue_test)
set_tests_properties(audio_queue PROPERTIES TIMEOUT 15)
