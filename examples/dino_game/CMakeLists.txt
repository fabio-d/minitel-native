cmake_minimum_required(VERSION 3.13)
include(../../lib/cmake/init.cmake)

find_package(Python3 COMPONENTS Interpreter)

project(dino_game C)
minitel_lib_init()

option(PROFILER "vary margin color according to code section being executed")
message(STATUS "Profiling mode: ${PROFILER}")
if(PROFILER)
  add_compile_definitions(PROFILER)
endif()

add_executable(dino_game
  display.c
  dynamic_area.c
  enemies.c
  floor_area.c
  main.c
  random.c
  score.c
  splash.c
  static_area.c
)

target_link_libraries(dino_game PRIVATE
  minitel_board
  minitel_keyboard
  minitel_timer
  minitel_video
)

set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})
target_include_directories(dino_game PRIVATE ${GENERATED_DIR})

add_custom_command(
  OUTPUT
    generated/floor_area-slices.h
  COMMAND
    ${Python3_EXECUTABLE}
    scripts/generate-floor.py
    --output ${GENERATED_DIR}/floor_area-slices.h
  WORKING_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
    scripts/generate-floor.py
  VERBATIM
)
add_custom_target(floor-data ALL
  DEPENDS
    generated/floor_area-slices.h
  SOURCES
    scripts/generate-floor.py
)
add_dependencies(dino_game floor-data)

add_custom_command(
  OUTPUT
    generated/splash-tiles.h
  COMMAND
    ${Python3_EXECUTABLE}
    scripts/generate-splash.py
    assets/SPLASH.png
    --output ${GENERATED_DIR}/splash-tiles.h
  WORKING_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
    scripts/generate-splash.py
    assets/SPLASH.png
  VERBATIM
)
add_custom_target(splash-data ALL
  DEPENDS
    generated/splash-tiles.h
  SOURCES
    scripts/generate-splash.py
)
add_dependencies(dino_game splash-data)

add_custom_command(
  OUTPUT
    generated/dynamic_area-sprites.h
  COMMAND
    ${Python3_EXECUTABLE}
    scripts/generate-sprites.py
    BIRD1=assets/BIRD1.png
    BIRD2=assets/BIRD2.png
    CACTUS=assets/CACTUS.png
    DINO1_LEADING:DINO1_TRAILING=assets/DINO1.png
    DINO2_LEADING:DINO2_TRAILING=assets/DINO2.png
    ROCK1=assets/ROCK1.png
    ROCK2=assets/ROCK2.png
    --output ${GENERATED_DIR}/dynamic_area-sprites.h
  WORKING_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
    scripts/generate-sprites.py
    assets/BIRD1.png
    assets/BIRD2.png
    assets/CACTUS.png
    assets/DINO1.png
    assets/DINO2.png
  VERBATIM
)
add_custom_target(dynamic-sprite-data ALL
  DEPENDS
    generated/dynamic_area-sprites.h
  SOURCES
    scripts/generate-sprites.py
)
add_dependencies(dino_game dynamic-sprite-data)

add_custom_command(
  OUTPUT
    generated/static_area-sprites.h
  COMMAND
    ${Python3_EXECUTABLE}
    scripts/generate-sprites.py
    CLOUD1L:CLOUD1M:CLOUD1R=assets/CLOUD1.png
    CLOUD2A=assets/CLOUD2A.png
    CLOUD2BTL:CLOUD2BTR:CLOUD2BBL:CLOUD2BBR=assets/CLOUD2B.png
    STAR1=assets/STAR1.png
    STAR2=assets/STAR2.png
    STAR3=assets/STAR3.png
    --output ${GENERATED_DIR}/static_area-sprites.h
  WORKING_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS
    scripts/generate-sprites.py
    assets/CLOUD1.png
    assets/CLOUD2A.png
    assets/CLOUD2B.png
    assets/STAR1.png
    assets/STAR2.png
    assets/STAR3.png
  VERBATIM
)
add_custom_target(static-sprite-data ALL
  DEPENDS
    generated/static_area-sprites.h
  SOURCES
    scripts/generate-sprites.py
)
add_dependencies(dino_game static-sprite-data)

minitel_add_bin_output(dino_game)
