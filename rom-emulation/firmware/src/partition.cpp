#include "partition.h"

#include <assert.h>
#include <boot/picobin.h>
#include <boot/uf2.h>
#include <hardware/sync.h>
#include <pico/assert.h>
#include <pico/bootrom.h>
#include <string.h>

#include <algorithm>
#include <tuple>

#include "romemu.h"

std::tuple<uint32_t, uint32_t> extract_base_offset_and_size(
    uint32_t permissions_and_location) {
  uint32_t first_sector = (permissions_and_location &
                           PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >>
                          PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
  uint32_t last_sector = (permissions_and_location &
                          PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >>
                         PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
  uint32_t num_sectors = last_sector - first_sector + 1;

  uint32_t base_offset = first_sector * FLASH_SECTOR_SIZE;
  uint32_t size = num_sectors * FLASH_SECTOR_SIZE;

  return {base_offset, size};
}

Partition::Partition() {
  base_offset = 0;
  size = 0;
  index = -1;
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

  std::tie(base_offset, size) =
      extract_base_offset_and_size(partition_info.permissions_and_location);
  index = rc;

  return true;
}

bool Partition::open_ab_other(uint partition_num) {
  uint other_partition_num;
  uint32_t partinfo[3];

  if (int rc = rom_get_b_partition(partition_num); rc >= 0) {
    other_partition_num = rc;
  } else {
    if (rom_get_partition_table_info(
            partinfo, count_of(partinfo),
            PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION |
                (partition_num << 24)) != count_of(partinfo)) {
      return false;
    }

    uint32_t permissions_and_flags = partinfo[2];
    uint8_t link_type =
        (permissions_and_flags & PICOBIN_PARTITION_FLAGS_LINK_TYPE_BITS) >>
        PICOBIN_PARTITION_FLAGS_LINK_TYPE_LSB;
    uint8_t link_value =
        (permissions_and_flags & PICOBIN_PARTITION_FLAGS_LINK_VALUE_BITS) >>
        PICOBIN_PARTITION_FLAGS_LINK_VALUE_LSB;
    if (link_type == PICOBIN_PARTITION_FLAGS_LINK_TYPE_A_PARTITION) {
      other_partition_num = link_value;
    } else {
      return false;
    }
  }

  if (rom_get_partition_table_info(
          partinfo, count_of(partinfo),
          PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION |
              (other_partition_num << 24)) != count_of(partinfo)) {
    return false;
  }

  uint32_t permissions_and_location = partinfo[1];
  std::tie(base_offset, size) =
      extract_base_offset_and_size(permissions_and_location);
  index = other_partition_num;

  return true;
}

uint Partition::get_index() const { return index; }

uint Partition::get_size() const { return size; }

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
  // Initialize our in-memory representation of the superblock to an empty one.
  memset(&superblock_contents, 0xFF, sizeof(Superblock));
  superblock_write_index = 0;

  if (!data_partition.open_with_family_id(DATA_FAMILY_ID)) {
    return false;
  }

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

void ConfigurationPartition::erase(uint slot_num) {
  assert(slot_num < 16);

  RomInfo &rom_slot = superblock_contents.rom_slots[slot_num];
  memset(&rom_slot, 0xFF, sizeof(rom_slot));

  flush_superblock_contents();
}

void ConfigurationPartition::set_wireless_config(const WirelessConfig &cfg) {
  superblock_contents.wireless = cfg;

  // We are about to clobber the buffer, so abort any ongoing flash operation.
  write_status = std::nullopt;
  flush_superblock_contents();
}

const ConfigurationPartition::WirelessConfig &
ConfigurationPartition::get_wireless_config() const {
  return superblock_contents.wireless;
}

OtaPartition::OtaPartition() {}

bool OtaPartition::open() {
  if (!next_partition.open_with_family_id(RP2350_ARM_S_FAMILY_ID)) {
    return false;
  }

  if (!current_partition.open_ab_other(next_partition.get_index())) {
    return false;
  }

  return true;
}

void OtaPartition::ota_begin() {
  write_status = WriteStatus{
      .write_cursor = 0,
  };
}

void OtaPartition::ota_data(uint8_t value) {
  if (write_status && write_status->write_cursor < next_partition.get_size()) {
    // Are we about to write to a different block than before?
    if (write_status->write_cursor % FLASH_SECTOR_SIZE == 0) {
      // Flush the previous block, unless we are just starting.
      if (write_status->write_cursor != 0) {
        uint32_t fw_block_num =
            (write_status->write_cursor - 1) / FLASH_SECTOR_SIZE;
        next_partition.erase_and_write_from_buffer(fw_block_num *
                                                   FLASH_SECTOR_SIZE);
      }

      // Initialize the buffer for new block.
      memset(next_partition.buffer, 0xFF, FLASH_SECTOR_SIZE);
    }

    next_partition.buffer[write_status->write_cursor++ % FLASH_SECTOR_SIZE] =
        value;
  }
}

void OtaPartition::ota_end() {
  if (write_status && write_status->write_cursor != 0) {
    // Flush the last block.
    uint32_t fw_block_num =
        (write_status->write_cursor - 1) / FLASH_SECTOR_SIZE;
    next_partition.erase_and_write_from_buffer(fw_block_num *
                                               FLASH_SECTOR_SIZE);

    // Invalidate the current partition by just zering out a magic value in the
    // header, to avoid disrupting the execution current program.
    invalidate_current_header();

    write_status = std::nullopt;
  }
}

// Symbols defined in the linker delimiting the firmware image.
extern "C" {
extern uint8_t __flash_binary_start, __flash_binary_end;
}

void OtaPartition::invalidate_current_header() {
  // The magic value we want to invalidate is the footer of the end block in the
  // block loop, stored at the very end of the firmware image.
  uint32_t magic_value_offset = &__flash_binary_end - 4 - &__flash_binary_start;
  uint32_t sector_offset = magic_value_offset % FLASH_SECTOR_SIZE;
  uint32_t sector_base = magic_value_offset - sector_offset;

  // Load the current contents of the sector containing the magic value.
  memcpy(current_partition.buffer, current_partition.get_contents(sector_base),
         FLASH_SECTOR_SIZE);

  // Assert that we are overwriting the expected magic value.
  const uint32_t expected_magic_value = PICOBIN_BLOCK_MARKER_END;
  hard_assert(memcmp(current_partition.buffer + sector_offset,
                     &expected_magic_value, 4) == 0);

  // Set it to zero and write back.
  memset(current_partition.buffer + sector_offset, 0, 4);
  current_partition.write_from_buffer(sector_base);
}
