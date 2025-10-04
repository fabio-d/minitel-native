cmake_minimum_required(VERSION 3.13)
include(../../lib/cmake/init.cmake)

find_package(Python3 COMPONENTS Interpreter)

project(image_gallery C)
minitel_lib_init()

add_executable(image_gallery
  main.c
)

target_include_directories(image_gallery PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(image_gallery PRIVATE
  minitel_board
  minitel_keyboard
  minitel_video
)

minitel_add_bin_output(image_gallery)

function(bundle_image NAME PNG_FILE)
  add_custom_command(
    OUTPUT
      ${NAME}.h
      ${NAME}-preview.png
    COMMAND
      ${Python3_EXECUTABLE}
    ARGS
      ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate-image-tiles.py
      --source-code-identifier ${NAME}
      --debug-output-image ${CMAKE_CURRENT_BINARY_DIR}/${NAME}-preview.png
      ${CMAKE_CURRENT_SOURCE_DIR}/${PNG_FILE}
      ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.h
      ${ARGN}  # forward extra arguments as-is
    DEPENDS
      scripts/generate-image-tiles.py
      ${CMAKE_CURRENT_SOURCE_DIR}/${PNG_FILE}
    VERBATIM
  )
  add_custom_target(bundled-image-${NAME}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.h
    SOURCES
      scripts/generate-image-tiles.py
      ${CMAKE_CURRENT_SOURCE_DIR}/${PNG_FILE}
  )
  add_dependencies(image_gallery bundled-image-${NAME})
endfunction()

bundle_image(image1 image1.png)
bundle_image(image2 image2.png)
bundle_image(image3 image3.png)
bundle_image(image4 image4.png --dither)
bundle_image(image5 image5.png --dither)
bundle_image(image6 image6.png --dither)
bundle_image(image7 image7.png --dither)
bundle_image(image8 image8.png --dither)
bundle_image(image9 image9.png --dither)
