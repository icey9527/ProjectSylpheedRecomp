cmake_minimum_required(VERSION 3.25)
project(sylpheed_bootstrap_tests LANGUAGES CXX)
if(NOT WIN32)
  message(FATAL_ERROR "The bootstrap ABI fixture currently requires Windows virtual memory")
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(rexglue 0.10.0 EXACT REQUIRED CONFIG)
add_executable(bootstrap_test bootstrap_test.cpp ../../src/patches/resources/development/bootstrap.cpp)
target_include_directories(bootstrap_test PRIVATE ../..)
target_link_libraries(bootstrap_test PRIVATE rex::runtime)
rexglue_apply_target_settings(bootstrap_test)
if(MSVC)
  # /EHsc assumes extern-C calls cannot throw, unlike the production Clang++
  # host. The fixture deliberately checks propagation through PPC C linkage.
  target_compile_options(bootstrap_test PRIVATE /EHa)
endif()
add_custom_command(TARGET bootstrap_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:bootstrap_test>
    $<TARGET_FILE_DIR:bootstrap_test> COMMAND_EXPAND_LISTS VERBATIM)
enable_testing()
add_test(NAME bootstrap COMMAND bootstrap_test)
