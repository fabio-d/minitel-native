set(MINITEL_ALL_SUPPORTED_MODELS
  nfz330
  nfz400
)

# Check that MINITEL_MODEL is set and valid.
set(MINITEL_MODEL "" CACHE STRING "Minitel model that will run the code")
if(MINITEL_MODEL IN_LIST MINITEL_ALL_SUPPORTED_MODELS)
  message(STATUS "Selected Minitel model ${MINITEL_MODEL}")
elseif(NOT MINITEL_MODEL)
  message(FATAL_ERROR "Please set cmake ... -DMINITEL_MODEL=...")
else()
  message(FATAL_ERROR "Unsupported Minitel model")
endif()

# Set sdcc-based toolchain.
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/sdcc-mcs51-toolchain.cmake)

# Store the absolute path to the "lib" directory in MINITEL_LIB_DIR.
get_filename_component(MINITEL_LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
message(STATUS "Using Minitel library at ${MINITEL_LIB_DIR}")

function(minitel_lib_init)
  find_package(Python3 COMPONENTS Interpreter)

  add_subdirectory(${MINITEL_LIB_DIR}/board/${MINITEL_MODEL} minitel_board)
  add_subdirectory(${MINITEL_LIB_DIR}/keyboard minitel_keyboard)
  add_subdirectory(${MINITEL_LIB_DIR}/timer minitel_timer)
  add_subdirectory(${MINITEL_LIB_DIR}/video minitel_video)
endfunction()

function(minitel_add_bin_output TARGET)
  get_target_property(BASENAME ${TARGET} OUTPUT_NAME)
  if(NOT FILENAME)
    get_target_property(BASENAME ${TARGET} NAME)
  endif()

  set(FULLPATH "${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}.bin")

  add_custom_command(
    TARGET ${TARGET}
    POST_BUILD
    COMMAND makebin -p -s 0x10000 $<TARGET_FILE:${TARGET}> ${FULLPATH}
    COMMENT "Generating ROM binary image ${BASENAME}.bin"
    VERBATIM
    BYPRODUCTS ${FULLPATH}
  )
endfunction()
