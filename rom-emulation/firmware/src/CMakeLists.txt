add_executable(rom-emulator
  cli-protocol.cpp
  led.cpp
  magic-io.cpp
  main.cpp
  mememu.cpp
  partition.cpp
  trace.cpp
)

pico_generate_pio_header(rom-emulator
  ${CMAKE_CURRENT_SOURCE_DIR}/mememu.pio
  ${CMAKE_CURRENT_SOURCE_DIR}/trace.pio
)

pico_set_program_name(rom-emulator "Minitel ROM Emulator")
pico_set_program_url(rom-emulator "https://github.com/fabio-d/minitel-native")

pico_enable_stdio_uart(rom-emulator 0)
pico_enable_stdio_usb(rom-emulator 1)

target_compile_definitions(rom-emulator PRIVATE
  "-DMINITEL_MODEL_FEATURE=\"Minitel model ${MINITEL_MODEL}\""
  "-DROM_EMULATOR_IS_INTERACTIVE=${ROM_EMULATOR_IS_INTERACTIVE}"
)

target_include_directories(rom-emulator PRIVATE
  .
  ../common
)

target_link_libraries(rom-emulator PRIVATE
  boot_uf2_headers
  hardware_dma
  hardware_flash
  hardware_pio
  hardware_pwm
  pico_bootrom
  pico_stdlib
)

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

if(ROM_EMULATOR_IS_INTERACTIVE)
  # Interactive mode requires partitioning. Call our script to create a special
  # UF2 file that, when flashed, will 1) initialize the partition table and
  # 2) fill all the partitions with their initial contents, in a single pass.
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/rom-emulator-full-install.uf2
    COMMAND ${Python3_EXECUTABLE}
    ARGS
      ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate-full-install.py
      --with-picotool $<TARGET_FILE:picotool>
      --pico-binary ${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_PROPERTY:rom-emulator,OUTPUT_NAME>.bin
      --output ${CMAKE_BINARY_DIR}/rom-emulator-full-install.uf2
    DEPENDS ../scripts/generate-full-install.py
    DEPENDS rom-emulator
    VERBATIM
  )
  add_custom_target(full-installation-image ALL
    DEPENDS ${CMAKE_BINARY_DIR}/rom-emulator-full-install.uf2
    SOURCES ../scripts/generate-full-install.py
  )

  # In addition, generate the regular UF2 file too, that can be used to update
  # the program A/B partition without losing the contents of the data partition.
  # Let's call it "rom-emulator-update-only.uf2" to highlight that it should
  # only be used for updating an existing installation.
  set_target_properties(rom-emulator PROPERTIES
    OUTPUT_NAME rom-emulator-update-only
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
  )

  if(PICO_CYW43_SUPPORTED)
    target_compile_definitions(rom-emulator PRIVATE
      "-DROM_EMULATOR_WITH_WIRELESS=1"
    )
    target_link_libraries(rom-emulator PRIVATE pico_cyw43_arch_lwip_poll)
  else()
    target_compile_definitions(rom-emulator PRIVATE
      "-DROM_EMULATOR_WITH_WIRELESS=0"
    )
  endif()
else()
  # Embedded mode is simpler and does not involve partitioning: just store the
  # regular output UF2 file in the top-level build directory, and call it
  # "rom-emulator-embedded.uf2".
  set_target_properties(rom-emulator PROPERTIES
    OUTPUT_NAME rom-emulator-embedded
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
  )

  # Make it absolute so that, if flashed over an interactive-mode installation,
  # the old partition table is automatically erased.
  pico_set_uf2_family(rom-emulator absolute)

  if(PICO_CYW43_SUPPORTED)
    target_link_libraries(rom-emulator PRIVATE pico_cyw43_arch_none)
  endif()

  # We never enable wireless in embedded mode.
  target_compile_definitions(rom-emulator PRIVATE
    "-DROM_EMULATOR_WITH_WIRELESS=0"
  )
endif()

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
  DEPENDS ${EMBED_ROM_FILE} ${EMBED_ROM_FILE_DEPENDS}
  VERBATIM
)
add_custom_target(embedded-rom-array
  DEPENDS embedded-rom-array.h
  SOURCES ../scripts/generate-rom-array.py
)
add_dependencies(rom-emulator embedded-rom-array)

pico_add_extra_outputs(rom-emulator)
