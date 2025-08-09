cmake_minimum_required(VERSION 3.22)


set(LIBRARY vrEmuTms9918)

set(CMAKE_C_STANDARD 11)

#option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

project(vrEmuTms9918)

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)

if (NOT BUILD_SHARED_LIBS)
 add_definitions(-DVR_EMU_TMS9918_STATIC)
endif()

if(MSVC)
  add_compile_options(/W4 /WX)
  add_compile_options(/arch:AVX2 /Ox)
else()
  #add_compile_options(-Wall -Wextra -Wpedantic -Werror)
  if (UNIX AND NOT CMAKE_CROSSCOMPILING)
    add_compile_options(-march=native)
  endif()
endif()

#include(CTest)

add_subdirectory(src)