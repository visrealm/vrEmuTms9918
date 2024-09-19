cmake_minimum_required(VERSION 3.12)

add_library(${LIBRARY} STATIC vrEmuTms9918.c vrEmuTms9918Util.c)

if (WIN32)
  if (BUILD_SHARED_LIBS)
     add_definitions(-DVR_TMS9918_EMU_COMPILING_DLL)
   endif()
endif()

target_include_directories (${LIBRARY} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${LIBRARY} PRIVATE pico_stdlib hardware_dma)