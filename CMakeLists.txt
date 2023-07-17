cmake_minimum_required(VERSION 3.12)

set(LIBRARY vrEmuTms9918)

project (${LIBRARY} C)

set(CMAKE_C_STANDARD 11)

add_library(${LIBRARY} src/vrEmuTms9918.c src/vrEmuTms9918Util.c)

target_compile_definitions(${LIBRARY} PUBLIC VR_TMS9918_EMU_STATIC)

target_include_directories (${LIBRARY} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${LIBRARY} PRIVATE pico_stdlib) 