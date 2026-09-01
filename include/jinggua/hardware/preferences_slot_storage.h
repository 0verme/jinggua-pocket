#pragma once

#include <cstddef>
#include <cstdint>

#include "jinggua/application/slot_storage.h"

namespace jinggua::hardware {

// SlotStorage backed by the ESP32 NVS partition via Arduino Preferences.
//
// Why NVS/Preferences:
//   - The StickS3 ships with a 24 KB NVS partition (default_8MB.csv), so no
//     partition table change or flash filesystem is required.
//   - NVS provides atomic per-entry writes with wear leveling, which matches
//     our corruption-recovery model: a torn write either lands as a whole
//     entry or not at all, and we additionally CRC every blob ourselves.
//   - Per-key storage lets us skip one corrupted record without touching the
//     others, and lets the index live as its own key.
//   - LittleFS would need a dedicated partition and a file per record (or a
//     manual offset bookkeeper) for no benefit at 26-byte records.
//
// Layout inside namespace "jinggua":
//   - key "idx": serialized HistoryIndex (see history_index.h)
//   - keys "rec_000".."rec_063": one serialized HistoryRecord each
class PreferencesSlotStorage final : public application::SlotStorage {
 public:
  std::size_t capacity() const noexcept override;

  bool readRecord(std::size_t slot, std::uint8_t* data, std::size_t capacity,
                  std::size_t& sizeOut) const noexcept override;
  bool writeRecord(std::size_t slot, const std::uint8_t* data,
                   std::size_t size) noexcept override;
  bool eraseRecord(std::size_t slot) noexcept override;

  bool readIndex(std::uint8_t* data, std::size_t capacity,
                 std::size_t& sizeOut) const noexcept override;
  bool writeIndex(const std::uint8_t* data, std::size_t size) noexcept override;

 private:
  void makeSlotKey(char* buffer, std::size_t slot) const noexcept;
};

}  // namespace jinggua::hardware
