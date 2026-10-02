cmake_minimum_required(VERSION 3.25)
project(sylpheed_performance_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
find_package(rexglue 0.10.0 EXACT REQUIRED CONFIG)
add_executable(performance_metrics_test
    performance_metrics_test.cpp
    ../../src/features/performance/frame_metrics.cpp
    ../../src/patches/graphics/development/frame_probe.cpp)
target_include_directories(performance_metrics_test PRIVATE ../.. ../../src)
target_link_libraries(performance_metrics_test PRIVATE rex::runtime)
rexglue_apply_target_settings(performance_metrics_test)
if(WIN32)
    add_custom_command(TARGET performance_metrics_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:performance_metrics_test>
            $<TARGET_FILE_DIR:performance_metrics_test>
        COMMAND_EXPAND_LISTS VERBATIM)
endif()
enable_testing()
add_test(NAME performance_metrics COMMAND performance_metrics_test)
