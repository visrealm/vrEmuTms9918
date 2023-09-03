cmake_minimum_required(VERSION 3.22)


set(CMAKE_C_STANDARD 11)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

project(vrEmuTms9918)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)

if (NOT BUILD_SHARED_LIBS)
 add_definitions(-DVR_EMU_TMS9918_STATIC)
endif()

include(CTest)

add_subdirectory(src)