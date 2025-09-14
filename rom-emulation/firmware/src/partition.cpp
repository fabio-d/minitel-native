#include "partition.h"

#include <assert.h>
#include <boot/picobin.h>
#include <boot/uf2.h>
#include <pico/assert.h>
#include <pico/bootrom.h>
#include <string.h>

#include "romemu.h"

static uint8_t workarea[4096];

Partition::Partition() {
  base_offset = 0;
  size = 0;
}

bool Partition::open_with_family_id(uint32_t family_id) {
  static_assert(sizeof(workarea) >= 3064,
                "Minimum workarea size for get_uf2_target_partition");

  resident_partition_t partition_info;
  int rc = rom_get_uf2_target_partition(workarea, sizeof(workarea), family_id,
                                        &partition_info);
  if (rc < 0) {
    return false;
  }

  uint32_t first_sector = (partition_info.permissions_and_location &
                           PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >>
                          PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
  uint32_t last_sector = (partition_info.permissions_and_location &
                          PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >>
                         PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
  uint32_t num_sectors = last_sector - first_sector + 1;

  base_offset = first_sector * FLASH_SECTOR_SIZE;
  size = num_sectors * FLASH_SECTOR_SIZE;
  return true;
}

const void *Partition::get_contents(uint32_t offset) const {
  return (const uint8_t *)XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + base_offset +
         offset;
}

ConfigurationPartition::ConfigurationPartition() {}

bool ConfigurationPartition::open() {
  if (!data_partition.open_with_family_id(DATA_FAMILY_ID)) {
    return false;
  }

  memcpy(&superblock_contents, data_partition.get_contents(0),
         sizeof(Superblock));

  return true;
}

const ConfigurationPartition::RomInfo &ConfigurationPartition::get_rom_info(
    uint slot_num) const {
  assert(slot_num < 16);
  return superblock_contents.rom_slots[slot_num];
}

const uint8_t *ConfigurationPartition::get_rom_contents(uint slot_num) const {
  assert(slot_num < 16);
  return static_cast<const uint8_t *>(
      data_partition.get_contents(ROM_BASE_OFFSET + slot_num * MAX_ROM_SIZE));
}
