cmake_minimum_required(VERSION 3.25)
project(sylpheed_binary_analysis LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
find_package(rexglue 0.10.0 EXACT REQUIRED CONFIG)
add_executable(dump_xex_image dump_xex_image.cpp)
target_link_libraries(dump_xex_image PRIVATE rex::runtime)
rexglue_apply_target_settings(dump_xex_image)
add_custom_command(TARGET dump_xex_image POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:dump_xex_image>
    $<TARGET_FILE_DIR:dump_xex_image> COMMAND_EXPAND_LISTS VERBATIM)
