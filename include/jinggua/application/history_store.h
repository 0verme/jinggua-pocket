#pragma once

#include <cstddef>
#include <cstdint>

#include "jinggua/domain/history_record.h"

namespace jinggua::application {

// History capacity / Flash footprint estimate
// ------------------------------------------
// Serialized record: 26 bytes (kHistoryRecordSerializedSize).
// Preferences (NVS) per-entry cost: 32-byte key/entry header + value slot
// padded to a 32-byte boundary → ≈ 96 bytes per record on Flash.
// 64 records × 96 B ≈ 6.1 KB plus one meta entry ≈ 96 B → ≈ 6.2 KB total.
// The StickS3 default_8MB.csv NVS partition is 24 KB, so history uses
// roughly a quarter of it, leaving ample headroom for future sync state.
// Write frequency: exactly one record + one meta write per completed
// divination (never per line, never per render), so NVS wear is bounded by
// how often the user actually casts.
constexpr std::size_t kHistoryCapacity = 64;

// Store interface. Implementations persist completed divinations in a
// bounded ring buffer (oldest evicted first). The interface is deliberately
// storage-agnostic so unit tests can run against an in-memory fake and the
// firmware can swap in NVS-backed storage.
class HistoryStore {
 public:
  virtual ~HistoryStore() = default;

  // Opens/initializes storage and recovers any prior history. Returns false
  // when storage is unavailable; callers must tolerate a failed begin
  // without crashing (new divinations may continue, they just won't persist).
  virtual bool begin() noexcept = 0;

  // Appends a completed divination. Assigns localRecordId and sessionOrder
  // (both monotonic, stable across reboots where possible). Returns false on
  // storage failure; the caller treats a failed add as non-fatal.
  virtual bool add(domain::HistoryRecord& record) noexcept = 0;

  // Number of stored records (0..capacity()).
  virtual std::size_t count() const noexcept = 0;

  // Reads the record at logical index (0 = oldest). Returns false when the
  // index is out of range or the underlying entry is corrupted.
  virtual bool get(std::size_t index,
                   domain::HistoryRecord& out) const noexcept = 0;

  // Erases all stored history.
  virtual void clear() noexcept = 0;

  virtual std::size_t capacity() const noexcept { return kHistoryCapacity; }

  // Next local record id to hand out; used by callers that want to display
  // the id before committing (e.g. "记录 #3"). Monotonic across reboots
  // where the store persists its counter.
  virtual std::uint32_t nextLocalRecordId() const noexcept = 0;
};

}  // namespace jinggua::application
