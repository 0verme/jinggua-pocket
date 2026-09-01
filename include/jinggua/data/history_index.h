#pragma once

#include <cstddef>
#include <cstdint>

#include "jinggua/domain/history_record.h"

namespace jinggua::data {

// Persistent metadata (index) block for a ring-buffer history store.
//
// A single small block, stored separately from the record slots, that tells
// the store where the ring starts (head), how many records are alive
// (count), and which identity/ordering counters to hand out next. It carries
// its own CRC so a torn meta write can be detected and rebuilt from the
// record slots.
struct HistoryIndex {
  std::uint8_t schemaVersion{domain::kHistorySchemaVersion};
  std::uint32_t head{0};
  std::uint32_t count{0};
  std::uint32_t nextLocalRecordId{1};
  std::uint32_t nextSessionOrder{1};
};

// Fixed serialized size: magic + schema + head + count + nextLocalId +
// nextSessionOrder + crc16.
constexpr std::size_t kHistoryIndexSerializedSize = 20;

std::size_t serializeHistoryIndex(const HistoryIndex& index, std::uint8_t* data,
                                  std::size_t capacity) noexcept;

// Parses a serialized index. Ok on success; UnsupportedSchema for a newer
// schema this firmware cannot interpret; CrcMismatch/TooShort for torn or
// garbage blocks.
domain::HistoryLoadStatus loadHistoryIndex(const std::uint8_t* data,
                                           std::size_t size,
                                           HistoryIndex& out) noexcept;

// Upgrades an older index in place. The first released layout is v1; v0 was
// the development-era layout (same fields, version byte only). Returns false
// only for versions with no known migration path.
bool migrateHistoryIndex(HistoryIndex& index) noexcept;

}  // namespace jinggua::data
