cmake_minimum_required (VERSION 3.7)
set(PROJECT1 GameBoyEmu)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_executable(${PROJECT1} main.cpp PPU.cpp PPU.h Emulator.cpp Emulator.h ROM.cpp ROM.h CPU.cpp CPU.h Logger.cpp Logger.h)

# Look up SDL2 and add the include directory to our include path
# include(FindPkgConfig)
# PKG_SEARCH_MODULE(SDL2 REQUIRED sdl2)
find_package(SDL2 REQUIRED COMPONENTS SDL2)
# add_executable(${PROJECT1} ${MY_SOURCES})
target_link_libraries(${PROJECT1} PRIVATE SDL2::SDL2)

# target_link_libraries(${PROJECT1} ${SDL2_LIBRARIES})
include_directories(${SDL2_INCLUDE_DIRS})
