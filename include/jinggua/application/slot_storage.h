#pragma once

#include <cstddef>
#include <cstdint>

namespace jinggua::application {

// Minimal byte-level storage abstraction for history persistence.
//
// A slot holds exactly one serialized HistoryRecord (fixed-size blob); the
// index (meta) holds the serialized HistoryIndex. Implementations are free
// to back this with NVS/Preferences, LittleFS, or a test double — the ring
// semantics, validation and recovery all live in RingHistoryStore, which
// only ever talks through this interface.
class SlotStorage {
 public:
  virtual ~SlotStorage() = default;

  // Number of record slots the backend can hold.
  virtual std::size_t capacity() const noexcept = 0;

  // Reads the record blob at `slot`. Returns false when the slot is empty,
  // unreadable, or the buffer is too small. On success `sizeOut` receives
  // the number of bytes read.
  virtual bool readRecord(std::size_t slot, std::uint8_t* data,
                          std::size_t capacity,
                          std::size_t& sizeOut) const noexcept = 0;

  // Writes a record blob at `slot`, overwriting any previous content.
  virtual bool writeRecord(std::size_t slot, const std::uint8_t* data,
                           std::size_t size) noexcept = 0;

  // Removes the record blob at `slot` (slot becomes empty).
  virtual bool eraseRecord(std::size_t slot) noexcept = 0;

  // Reads the index blob. Returns false when absent/unreadable/buffer small.
  virtual bool readIndex(std::uint8_t* data, std::size_t capacity,
                         std::size_t& sizeOut) const noexcept = 0;

  // Writes the index blob, overwriting any previous content.
  virtual bool writeIndex(const std::uint8_t* data,
                          std::size_t size) noexcept = 0;
};

}  // namespace jinggua::application
