#ifndef ROM_EMULATION_FIRMWARE_SRC_PARTITION_H
#define ROM_EMULATION_FIRMWARE_SRC_PARTITION_H

#include <hardware/flash.h>
#include <pico/types.h>

// Raw partition access.
class Partition {
 public:
  Partition();

  // Initializes this object by calling "rom_get_uf2_target_partition" with the
  // given family ID to discover the boundaries of the partition.
  bool open_with_family_id(uint32_t family_id);

  // Obtains a pointer to the memory-mapped flash sector.
  const void *get_contents(uint32_t offset) const;

 private:
  uint32_t base_offset, size;
};

#endif
