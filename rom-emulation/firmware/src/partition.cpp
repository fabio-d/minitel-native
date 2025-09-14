#include "partition.h"

#include <assert.h>
#include <boot/picobin.h>
#include <boot/uf2.h>
#include <hardware/sync.h>
#include <pico/assert.h>
#include <pico/bootrom.h>
#include <string.h>

#include <algorithm>

#include "romemu.h"

Partition::Partition() {
  base_offset = 0;
  size = 0;
}

bool Partition::open_with_family_id(uint32_t family_id) {
  static_assert(sizeof(buffer) >= 3064,
                "Minimum workarea size for get_uf2_target_partition");

  resident_partition_t partition_info;
  int rc = rom_get_uf2_target_partition(buffer, sizeof(buffer), family_id,
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

void Partition::erase(uint32_t sector_offset) {
  hard_assert(validate_sector_offset(sector_offset), "invalid sector_offset");

  uint32_t status = save_and_disable_interrupts();
  flash_range_erase(base_offset + sector_offset, FLASH_SECTOR_SIZE);
  restore_interrupts(status);
}

void Partition::write_from_buffer(uint32_t sector_offset) {
  hard_assert(validate_sector_offset(sector_offset), "invalid sector_offset");

  uint32_t status = save_and_disable_interrupts();
  flash_range_program(base_offset + sector_offset, buffer, FLASH_SECTOR_SIZE);
  restore_interrupts(status);
}

void Partition::erase_and_write_from_buffer(uint32_t sector_offset) {
  const uint8_t *old_contents =
      static_cast<const uint8_t *>(get_contents(sector_offset));

  bool needs_erase = false;
  bool all_equal = true;
  for (uint i = 0; i < FLASH_SECTOR_SIZE; i++) {
    if (buffer[i] & ~old_contents[i]) {  // is any bit going from 0 to 1?
      needs_erase = true;
    }
    if (buffer[i] != old_contents[i]) {
      all_equal = false;
    }
  }

  if (!all_equal) {
    if (needs_erase) {
      erase(sector_offset);
    }
    write_from_buffer(sector_offset);
  }
}

bool Partition::validate_sector_offset(uint32_t sector_offset) {
  if (size == 0) {
    return false;  // The partition has not been opened yet.
  }
  if (sector_offset % FLASH_SECTOR_SIZE) {
    return false;  // The given offset is not properly aligned.
  }
  if (sector_offset >= size) {
    return false;  // Offset is past the end of the partition.
  }
  return true;
}

ConfigurationPartition::ConfigurationPartition() {}

bool ConfigurationPartition::open() {
  if (!data_partition.open_with_family_id(DATA_FAMILY_ID)) {
    return false;
  }

  // Initialize our in-memory representation of the superblock to an empty one.
  memset(&superblock_contents, 0xFF, sizeof(Superblock));
  superblock_write_index = 0;

  // Load the one with the lowest generation_counter.
  // Note: the candidate's generation_counter must be strictly lower than
  // superblock_contents. This, given how we initialized superblock_contents,
  // rules out loading superblocks with generation_counter == 0xFFFFFFFF.
  for (uint i = 0; i < NUM_SUPERBLOCKS; i++) {
    uint32_t candidate_offset = i * FLASH_SECTOR_SIZE;
    const Superblock *candidate = static_cast<const Superblock *>(
        data_partition.get_contents(candidate_offset));

    if (candidate->generation_counter <
        superblock_contents.generation_counter) {
      memcpy(&superblock_contents, candidate, sizeof(Superblock));
      superblock_write_index = (i + 1) % NUM_SUPERBLOCKS;
    }
  }

  return true;
}

void ConfigurationPartition::flush_superblock_contents() {
  memset(data_partition.buffer, 0xFF, FLASH_SECTOR_SIZE);

  superblock_contents.generation_counter--;
  memcpy(data_partition.buffer, &superblock_contents, sizeof(Superblock));

  data_partition.erase_and_write_from_buffer(superblock_write_index *
                                             FLASH_SECTOR_SIZE);

  if (++superblock_write_index == NUM_SUPERBLOCKS) {
    superblock_write_index = 0;
  }
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

void ConfigurationPartition::write_begin(uint slot_num, uint8_t name_length,
                                         const char *name) {
  assert(slot_num < 16);

  RomInfo &rom_slot = superblock_contents.rom_slots[slot_num];
  memset(&rom_slot, 0xFF, sizeof(rom_slot));

  // Set the name, but leave the size to all 1's (i.e. not present): the real
  // size will be written at the end, to commit the ROM.
  rom_slot.name_length = std::min<uint8_t>(name_length, sizeof(rom_slot.name));
  memcpy(rom_slot.name, name, rom_slot.name_length);

  flush_superblock_contents();

  write_status = WriteStatus{
      .slot_num = slot_num,
      .write_cursor = 0,
  };
}

void ConfigurationPartition::write_data(uint8_t value) {
  if (write_status && write_status->write_cursor < MAX_ROM_SIZE) {
    // Are we about to write to a different block than before?
    if (write_status->write_cursor % FLASH_SECTOR_SIZE == 0) {
      // Flush the previous block, unless we are just starting.
      if (write_status->write_cursor != 0) {
        uint32_t rom_block_num =
            (write_status->write_cursor - 1) / FLASH_SECTOR_SIZE;
        data_partition.erase_and_write_from_buffer(
            ROM_BASE_OFFSET + write_status->slot_num * MAX_ROM_SIZE +
            rom_block_num * FLASH_SECTOR_SIZE);
      }

      // Initialize the buffer for new block.
      memset(data_partition.buffer, 0xFF, FLASH_SECTOR_SIZE);
    }

    data_partition.buffer[write_status->write_cursor++ % FLASH_SECTOR_SIZE] =
        value;
  }
}

void ConfigurationPartition::write_end() {
  if (write_status && write_status->write_cursor != 0) {
    // Flush the last block.
    uint32_t rom_block_num =
        (write_status->write_cursor - 1) / FLASH_SECTOR_SIZE;
    data_partition.erase_and_write_from_buffer(
        ROM_BASE_OFFSET + write_status->slot_num * MAX_ROM_SIZE +
        rom_block_num * FLASH_SECTOR_SIZE);

    RomInfo &rom_slot = superblock_contents.rom_slots[write_status->slot_num];
    rom_slot.size = write_status->write_cursor;

    write_status = std::nullopt;

    flush_superblock_contents();
  }
}
