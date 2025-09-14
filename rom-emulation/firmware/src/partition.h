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

// Mediates access to the data partition.
//
// The data partition is organized as follows:
// - The first sector contains the Superblock data structure.
// - Immediately after, the ROMs' contents follow. Each ROM slot has a fixed
//   MAX_ROM_SIZE of bytes reserved for it, even if the slot if currently empty
//   or its stored ROM is smaller than that.
class ConfigurationPartition {
 public:
  struct [[gnu::packed]] RomInfo {
    uint32_t size;  // UINT32_MAX = not present
    uint8_t name_length;
    char name[126];

    inline bool is_present() const { return size != UINT32_MAX; }
  };

  ConfigurationPartition();

  // Locates and reads the current configuration.
  bool open();

  const RomInfo &get_rom_info(uint slot_num) const;
  const uint8_t *get_rom_contents(uint slot_num) const;

 private:
  // Handle to the underlying partition.
  Partition data_partition;

  // The Superblock contains the catalog of the stored ROMs:
  struct [[gnu::packed]] Superblock {
    RomInfo rom_slots[16];
  };
  Superblock superblock_contents;

  // Constants defining the partition's layout:
  static_assert(sizeof(Superblock) <= FLASH_SECTOR_SIZE,
                "The Superblock data structure does not fit in one sector");
  static constexpr uint32_t ROM_BASE_OFFSET = FLASH_SECTOR_SIZE;
};

#endif
