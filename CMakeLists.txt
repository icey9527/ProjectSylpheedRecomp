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
)

if(WIN32)
    add_executable(project_sylpheed WIN32 ${PROJECTSYLPHEED_SOURCES})
else()
    add_executable(project_sylpheed ${PROJECTSYLPHEED_SOURCES})
endif()

rexglue_setup_target(project_sylpheed)
