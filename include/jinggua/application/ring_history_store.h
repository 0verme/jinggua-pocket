#pragma once

#include <array>
#include <cstddef>

#include "jinggua/application/history_store.h"
#include "jinggua/application/slot_storage.h"
#include "jinggua/data/history_index.h"
#include "jinggua/domain/history_record.h"

namespace jinggua::application {

// HistoryStore implementation that manages a ring buffer over a fixed-size
// SlotStorage. All ring bookkeeping, schema validation, migration, and
// corruption recovery lives here, fully testable without hardware.
//
// The ring uses physical slots 0..capacity-1 on the storage backend. A
// persistent index (HistoryIndex) records head, count, and identity
// counters. On begin() the index is loaded and validated; if the index is
// unreadable the store falls back to a full slot scan, collecting valid
// records sorted by sessionOrder.
//
// Corrupted records (CRC mismatch) are silently skipped during get().
// A corrupted index triggers a full rebuild that preserves every valid
// record while discarding the unreadable index.
class RingHistoryStore final : public HistoryStore {
 public:
  explicit RingHistoryStore(SlotStorage& storage) noexcept;

  // ── HistoryStore ────────────────────────────────────────────────────
  bool begin() noexcept override;
  bool add(domain::HistoryRecord& record) noexcept override;
  std::size_t count() const noexcept override;
  bool get(std::size_t index,
           domain::HistoryRecord& out) const noexcept override;
  void clear() noexcept override;
  std::size_t capacity() const noexcept override {
    return storage_.capacity();
  }
  std::uint32_t nextLocalRecordId() const noexcept override;
  // ────────────────────────────────────────────────────────────────────

 private:
  // Internal helpers for slot index arithmetic.
  std::size_t tailSlot() const noexcept;
  std::size_t physicalSlot(std::size_t logicalIndex) const noexcept;

  // Attempt to load the stored index. Returns true when the index was
  // successfully loaded and validated; false when it is missing, corrupt,
  // or unreadable (caller should run rebuildFromSlots instead).
  bool loadIndex() noexcept;

  // Full rebuild: scan all slots, collect valid records, sort by
  // (sessionOrder, localRecordId), reconstruct head/count/nextIds.
  // Returns true when the store is usable afterwards (even with zero
  // records); false would mean the backend itself is unreadable.
  bool rebuildFromSlots() noexcept;

  // Persist the current index to storage.
  bool persistIndex() noexcept;

  // Validate a single slot's record blob. Returns Ok on success; on
  // CrcMismatch/TooShort the slot is treated as empty; on
  // UnsupportedSchema the record is skipped (unknown newer version).
  domain::HistoryLoadStatus validateSlot(std::size_t slot,
                                         domain::HistoryRecord& out) const noexcept;

  SlotStorage& storage_;
  data::HistoryIndex index_{};
  bool healthy_{false};
};

}  // namespace jinggua::application