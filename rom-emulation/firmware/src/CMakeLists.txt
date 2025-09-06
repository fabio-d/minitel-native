add_executable(rom-emulator
  cli-protocol.cpp
  led.cpp
  main.cpp
  romemu.cpp
  trace.cpp
)

pico_generate_pio_header(rom-emulator
  ${CMAKE_CURRENT_SOURCE_DIR}/romemu.pio
  ${CMAKE_CURRENT_SOURCE_DIR}/trace.pio
)

pico_set_program_name(rom-emulator "Minitel ROM Emulator")
pico_set_program_url(rom-emulator "https://github.com/fabio-d/minitel-native")

pico_enable_stdio_uart(rom-emulator 0)
pico_enable_stdio_usb(rom-emulator 1)

target_compile_definitions(rom-emulator PRIVATE
  "-DMINITEL_MODEL_FEATURE=\"Minitel model ${MINITEL_MODEL}\""
  "-DOPERATING_MODE_FEATURE=\"Operating in ${OPERATING_MODE} mode\""
)

target_link_libraries(rom-emulator PRIVATE
  hardware_pio
  hardware_pwm
  pico_multicore
  pico_stdlib
)

if(PICO_CYW43_SUPPORTED)
  target_link_libraries(rom-emulator PRIVATE
    pico_cyw43_arch_none
  )
endif()

# Generate the pin map.
add_custom_command(
  OUTPUT pin-map.h
  COMMAND
    ${Python3_EXECUTABLE}
  ARGS
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate-pin-map.py
    ${PINOUT}
    pin-map.h
  DEPENDS ../scripts/generate-pin-map.py
  VERBATIM
)
add_custom_target(pin-map
  DEPENDS pin-map.h
  SOURCES ../scripts/generate-pin-map.py
)
add_dependencies(rom-emulator pin-map)

# Generate our custom linker script by patching the one in the SDK.
add_custom_command(
  OUTPUT linker-script.ld
  COMMAND
    ${Python3_EXECUTABLE}
  ARGS
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate-linker-script.py
    ${PICO_SDK_PATH}
    linker-script.ld
  DEPENDS ../scripts/generate-linker-script.py
  DEPENDS ${PICO_SDK_PATH}/src/rp2_common/pico_crt0/rp2350/memmap_default.ld
  VERBATIM
)
add_custom_target(linker-script
  DEPENDS linker-script.ld
  SOURCES ../scripts/generate-linker-script.py
)
add_dependencies(rom-emulator linker-script)
pico_set_linker_script(rom-emulator
  ${CMAKE_CURRENT_BINARY_DIR}/linker-script.ld
)

# Store the output UF2 file in the top-level build directory, and call it
# "rom-emulator-embedded.uf2".
set_target_properties(rom-emulator PROPERTIES
  OUTPUT_NAME rom-emulator-embedded
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Convert the embedded ROM binary into a C array.
add_custom_command(
  OUTPUT embedded-rom-array.h
  COMMAND
    ${Python3_EXECUTABLE}
  ARGS
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate-rom-array.py
    ${EMBED_ROM_FILE}
    embedded-rom-array.h
    EMBEDDED_ROM
  DEPENDS ../scripts/generate-rom-array.py
  DEPENDS ${EMBED_ROM_FILE}
  VERBATIM
)
add_custom_target(embedded-rom-array
  DEPENDS embedded-rom-array.h
  SOURCES ../scripts/generate-rom-array.py
)
add_dependencies(rom-emulator embedded-rom-array)

pico_add_extra_outputs(rom-emulator)
