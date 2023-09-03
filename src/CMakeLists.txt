

add_library(vrEmuTms9918 vrEmuTms9918.c)
add_library(vrEmuTms9918Util vrEmuTms9918Util.c)

if (WIN32)
  if (BUILD_SHARED_LIBS)
     add_definitions(-DVR_TMS9918_EMU_COMPILING_DLL)
   endif()
endif()

target_include_directories (vrEmuTms9918 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(vrEmuTms9918Util PUBLIC vrEmuTms9918)
