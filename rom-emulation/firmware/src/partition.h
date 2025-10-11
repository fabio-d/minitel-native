#ifndef ROM_EMULATION_FIRMWARE_SRC_PARTITION_H
#define ROM_EMULATION_FIRMWARE_SRC_PARTITION_H

#include <hardware/flash.h>
#include <pico/types.h>

#include <optional>

// Raw partition access.
class Partition {
 public:
  Partition();

  // Initializes this object by calling "rom_get_uf2_target_partition" with the
  // given family ID to discover the boundaries of the partition.
  bool open_with_family_id(uint32_t family_id);

  // Given a partition index, opens the other partition in a A/B configuration.
  bool open_ab_other(uint partition_num);

  // Get the index of the detected partition.
  uint get_index() const;

  // Get the size of the detected partition.
  uint get_size() const;

  // Obtains a pointer to the memory-mapped flash sector.
  const void *get_contents(uint32_t offset) const;

  // Erases the given flash sector.
  //
  // This turns all bits in the sector to 1.
  void erase(uint32_t sector_offset);

  // Writes the buffer into the given flash sector.
  //
  // The new sector contents will be the result of a bitwise AND operation of
  // its current contents and the buffer. In other words, it can only change
  // 1s into 0s. Use erase() to set the flash sector to all 1s.
  void write_from_buffer(uint32_t sector_offset);

  // Combines erase and write_from_buffer, skipping the erase is not needed.
  void erase_and_write_from_buffer(uint32_t sector_offset);

  alignas(max_align_t) uint8_t buffer[FLASH_SECTOR_SIZE];

 private:
  bool validate_sector_offset(uint32_t sector_offset);

  uint32_t base_offset, size;
  uint index;
};

// Mediates access to the data partition.
//
// The data partition is organized as follows:
// - The first NUM_SUPERBLOCKS sectors contain Superblock data structures, one
//   per sector. Only one of them is current, and the other ones are ignored.
//   Which one is current is determined when the partition is opened by locating
//   the one with the lowest generation_counter.
// - Immediately after, the ROMs' contents follow. Each ROM slot has a fixed
//   MAX_ROM_SIZE of bytes reserved for it, even if the slot if currently empty
//   or its stored ROM is smaller than that.
//
// In order to 1) tolerate power cuts during updates and 2) implement a very
// minimal form of wear levelling, new versions of the Superblock are written
// into a sector (within the first NUM_SUPERBLOCKS) different from the current
// one, in a round-robin fashion.
class ConfigurationPartition {
 public:
  struct [[gnu::packed]] RomInfo {
    uint32_t size;  // UINT32_MAX = not present
    uint8_t name_length;
    char name[126];

    inline bool is_present() const { return size != UINT32_MAX; }
  };

  struct [[gnu::packed]] WirelessConfig {
    enum : uint8_t {
      OpenNetwork = 0,
      WpaNetwork = 1,
      NotConfigured = 0xFF,
    } type;

    char ssid[32 + 1];  // if set, NUL-terminated
    char psk[63 + 1];   // if set, NUL-terminated

    inline bool is_configured() const { return type != NotConfigured; }
    inline bool is_open() const { return type == OpenNetwork; }
  };

  ConfigurationPartition();

  // Locates and reads the current configuration.
  bool open();

  const RomInfo &get_rom_info(uint slot_num) const;
  const uint8_t *get_rom_contents(uint slot_num) const;

  void write_begin(uint slot_num, uint8_t name_length, const char *name);
  void write_data(uint8_t value);
  void write_end();

  void set_wireless_config(const WirelessConfig &cfg);
  const WirelessConfig &get_wireless_config() const;

 private:
  // Persists the value of superblock_contents to flash.
  void flush_superblock_contents();

  // Handle to the underlying partition.
  Partition data_partition;

  // The Superblock contains the catalog of the stored ROMs:
  struct [[gnu::packed]] Superblock {
    uint32_t generation_counter;  // less is newer, 0xFFFFFFFF = invalid.
    RomInfo rom_slots[16];
    WirelessConfig wireless;
  };
  Superblock superblock_contents;
  uint superblock_write_index;  // where to write the next superblock update.

  struct WriteStatus {
    uint slot_num;
    uint32_t write_cursor;
  };
  std::optional<WriteStatus> write_status;

  // Constants defining the partition's layout:
  static_assert(sizeof(Superblock) <= FLASH_SECTOR_SIZE,
                "The Superblock data structure does not fit in one sector");
  static constexpr uint32_t NUM_SUPERBLOCKS = 16;
  static constexpr uint32_t ROM_BASE_OFFSET =
      FLASH_SECTOR_SIZE * NUM_SUPERBLOCKS;
};

// Mediates access to the A/B partitions containing the Pico's own firmware.
//
// The A/B partitioning scheme makes it possible to write the firmware for the
// next boot into one partition, while the current firmware is still running
// from the other one.
class OtaPartition {
 public:
  OtaPartition();

  // Locates the current and next partitions.
  bool open();

  void ota_begin();
  void ota_data(uint8_t value);
  void ota_end();

 private:
  void invalidate_current_header();

  //  Handle to the underlying partitions.
  Partition current_partition, next_partition;

  struct WriteStatus {
    uint32_t write_cursor;
  };
  std::optional<WriteStatus> write_status;
};

#endif
