cmake_minimum_required(VERSION 3.10)
set(CMAKE_FIND_FRAMEWORK NEVER)
set(PROJECT_NAME jauto)
project(${PROJECT_NAME} VERSION 1.0)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
configure_file(jauto_config.h.in jauto_config.h)
find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})
file(GLOB COMMAND_SOURCES src/commands/*.c)
add_library(${PROJECT_NAME} SHARED
    src/command.c src/file_out.c src/jauto.c src/logger.c src/pipe_in.c
    ${COMMAND_SOURCES}
    src/sds/sds.c)
