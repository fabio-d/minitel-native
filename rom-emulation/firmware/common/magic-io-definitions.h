#ifndef ROM_EMULATION_FIRMWARE_COMMON_MAGIC_IO_DEFINITIONS_H
#define ROM_EMULATION_FIRMWARE_COMMON_MAGIC_IO_DEFINITIONS_H

#include <stdint.h>

// The Pico can ask the Minitel to be in one of these states.
typedef enum {
  MAGIC_IO_DESIRED_STATE_MAIN_MENU,        // In the main menu.
  MAGIC_IO_DESIRED_STATE_BOOT_TRAMPOLINE,  // In the boot trampoline.
  MAGIC_IO_DESIRED_STATE_PARTITION_ERROR,  // Partitioning error.
} MAGIC_IO_DESIRED_STATE_t;

typedef struct {
  // When read, these ROM locations trigger actions on the Pico.
  volatile struct ACTIVE_AREA {
    // For resetting the state of the magic I/O interface:
    // - Read reset_generation_count once.
    // - Poll reset_generation_count until it changes.
    volatile uint8_t reset_generation_count;

    // For telling the Pico that the user requested to proceed to the ROM:
    // - Poll user_requested_boot until it goes to 0.
    volatile uint8_t user_requested_boot;
  } a;

  // Reading these locations does not send any signal.
  volatile struct PASSIVE_AREA {
    uint8_t desired_state;  // actual type MAGIC_IO_DESIRED_STATE_t.
  } p;
} MAGIC_IO_t;

#endif
