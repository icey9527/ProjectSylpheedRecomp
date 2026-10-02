cmake_minimum_required(VERSION 3.25)
project(sylpheed_movie_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(rexglue 0.10.0 EXACT REQUIRED CONFIG)
add_executable(movie_fallback_test
    movie_fallback_test.cpp
    ../../src/patches/movie/development/movie_fallback.cpp)
target_include_directories(movie_fallback_test PRIVATE ../..)
target_link_libraries(movie_fallback_test PRIVATE rex::runtime)
rexglue_apply_target_settings(movie_fallback_test)
if(WIN32)
    add_custom_command(TARGET movie_fallback_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:movie_fallback_test>
            $<TARGET_FILE_DIR:movie_fallback_test>
        COMMAND_EXPAND_LISTS VERBATIM)
endif()
enable_testing()
add_test(NAME movie_fallback COMMAND movie_fallback_test)
