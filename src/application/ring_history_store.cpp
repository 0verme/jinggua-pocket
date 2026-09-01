#include "jinggua/application/ring_history_store.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace jinggua::application {

RingHistoryStore::RingHistoryStore(SlotStorage& storage) noexcept
    : storage_(storage) {}

// ---------------------------------------------------------------------------
// Slot arithmetic
// ---------------------------------------------------------------------------
std::size_t RingHistoryStore::tailSlot() const noexcept {
  return (index_.head + index_.count) % storage_.capacity();
}

std::size_t RingHistoryStore::physicalSlot(
    std::size_t logicalIndex) const noexcept {
  return (index_.head + logicalIndex) % storage_.capacity();
}

// ---------------------------------------------------------------------------
// Index management
// ---------------------------------------------------------------------------
bool RingHistoryStore::loadIndex() noexcept {
  std::array<std::uint8_t, data::kHistoryIndexSerializedSize> blob{};
  std::size_t size = 0;
  if (!storage_.readIndex(blob.data(), blob.size(), size)) {
    return false;
  }
  data::HistoryIndex candidate{};
  const auto status = data::loadHistoryIndex(blob.data(), size, candidate);
  if (status != domain::HistoryLoadStatus::Ok) {
    return false;
  }
  // Guard against a malicious/bogus index pointing outside the slot range.
  if (candidate.count > storage_.capacity() ||
      candidate.head >= storage_.capacity()) {
    return false;
  }
  data::migrateHistoryIndex(candidate);
  index_ = candidate;
  return true;
}

bool RingHistoryStore::persistIndex() noexcept {
  std::array<std::uint8_t, data::kHistoryIndexSerializedSize> blob{};
  const auto size = data::serializeHistoryIndex(index_, blob.data(),
                                                blob.size());
  return storage_.writeIndex(blob.data(), size);
}

// ---------------------------------------------------------------------------
// Slot validation
// ---------------------------------------------------------------------------
domain::HistoryLoadStatus RingHistoryStore::validateSlot(
    std::size_t slot, domain::HistoryRecord& out) const noexcept {
  std::array<std::uint8_t, domain::kHistoryRecordSerializedSize> blob{};
  std::size_t size = 0;
  if (!storage_.readRecord(slot, blob.data(), blob.size(), size)) {
    return domain::HistoryLoadStatus::TooShort;  // empty/unreadable slot
  }
  auto status = domain::loadHistoryRecord(blob.data(), size, out);
  if (status != domain::HistoryLoadStatus::Ok) {
    return status;
  }
  // Migrate older schemas in memory. The upgraded bytes are only persisted
  // lazily, when this slot is next overwritten by add().
  if (out.schemaVersion < domain::kHistorySchemaVersion) {
    if (!domain::migrateHistoryRecord(out)) {
      return domain::HistoryLoadStatus::UnsupportedSchema;
    }
  }
  return domain::HistoryLoadStatus::Ok;
}

// ---------------------------------------------------------------------------
// Recovery
// ---------------------------------------------------------------------------
bool RingHistoryStore::rebuildFromSlots() noexcept {
  // Scan every slot, collecting valid records.
  std::array<domain::HistoryRecord, 64> found{};
  std::size_t foundCount = 0;

  for (std::size_t slot = 0; slot < storage_.capacity(); ++slot) {
    domain::HistoryRecord record;
    if (validateSlot(slot, record) == domain::HistoryLoadStatus::Ok) {
      if (foundCount < found.size()) {
        found[foundCount] = record;
        ++foundCount;
      }
    }
    // Corrupt / unknown-newer / empty slots are skipped: one bad record must
    // never take down the rest of the history.
  }

  // Sort by (sessionOrder, localRecordId) to restore chronological order even
  // though the ring index was lost. Small N (≤ capacity), insertion sort.
  for (std::size_t i = 1; i < foundCount; ++i) {
    const auto key = found[i];
    std::size_t j = i;
    while (j > 0 && (found[j - 1].sessionOrder > key.sessionOrder ||
                     (found[j - 1].sessionOrder == key.sessionOrder &&
                      found[j - 1].localRecordId > key.localRecordId))) {
      found[j] = found[j - 1];
      --j;
    }
    found[j] = key;
  }

  data::HistoryIndex rebuilt{};
  rebuilt.schemaVersion = domain::kHistorySchemaVersion;
  rebuilt.head = 0;
  rebuilt.count = static_cast<std::uint32_t>(foundCount);
  for (std::size_t i = 0; i < foundCount; ++i) {
    // Re-write each valid record into a contiguous slot so the rebuilt head
    // and count describe the new layout deterministically.
    std::array<std::uint8_t, domain::kHistoryRecordSerializedSize> blob{};
    const auto size =
        domain::serializeHistoryRecord(found[i], blob.data(), blob.size());
      storage_.writeRecord(i, blob.data(), size);

    if (found[i].localRecordId >= rebuilt.nextLocalRecordId) {
      rebuilt.nextLocalRecordId = found[i].localRecordId + 1;
    }
    if (found[i].sessionOrder >= rebuilt.nextSessionOrder) {
      rebuilt.nextSessionOrder = found[i].sessionOrder + 1;
    }
  }

  // Clear the remainder of the slot space.
  for (std::size_t slot = foundCount; slot < storage_.capacity(); ++slot) {
    storage_.eraseRecord(slot);
  }

  index_ = rebuilt;
  if (!persistIndex()) {
    // Index write failed; keep in-memory state consistent anyway so the
    // device can continue casting this session.
    healthy_ = true;
    return true;
  }
  healthy_ = true;
  return true;
}

// ---------------------------------------------------------------------------
// HistoryStore
// ---------------------------------------------------------------------------
bool RingHistoryStore::begin() noexcept {
  healthy_ = false;
  index_ = data::HistoryIndex{};

  if (loadIndex()) {
    healthy_ = true;
    return true;
  }

  // Index missing or corrupt → try a full scan rebuild. A fresh device has
  // no records, so rebuild finds nothing and healthy_ becomes true with an
  // empty store.
  return rebuildFromSlots();
}

bool RingHistoryStore::add(domain::HistoryRecord& record) noexcept {
  if (storage_.capacity() == 0) {
    return false;
  }

  record.localRecordId = index_.nextLocalRecordId;
  record.sessionOrder = index_.nextSessionOrder;
  record.schemaVersion = domain::kHistorySchemaVersion;
  ++index_.nextLocalRecordId;
  ++index_.nextSessionOrder;

  std::array<std::uint8_t, domain::kHistoryRecordSerializedSize> blob{};
  const auto size = domain::serializeHistoryRecord(record, blob.data(),
                                                   blob.size());

  const std::size_t slot = tailSlot();
  if (!storage_.writeRecord(slot, blob.data(), size)) {
    // Roll back the counters so a failed persist does not skip ids.
    --index_.nextLocalRecordId;
    --index_.nextSessionOrder;
    return false;
  }

  if (index_.count < storage_.capacity()) {
    ++index_.count;
  } else {
    // Ring full: we just overwrote the oldest slot, advance head.
    index_.head = (index_.head + 1) % storage_.capacity();
  }

  if (!persistIndex()) {
    // Record is durable but the index update failed. The store stays
    // usable; a later begin() scan will reconstruct the ring.
    healthy_ = true;
    return true;
  }
  return true;
}

std::size_t RingHistoryStore::count() const noexcept { return index_.count; }

bool RingHistoryStore::get(std::size_t index,
                           domain::HistoryRecord& out) const noexcept {
  if (index >= index_.count) {
    return false;
  }
  const auto slot = physicalSlot(index);
  const auto status = validateSlot(slot, out);
  if (status != domain::HistoryLoadStatus::Ok) {
    // A corrupted record reads back as missing; skip it without disturbing
    // the rest of the history.
    return false;
  }
  return true;
}

void RingHistoryStore::clear() noexcept {
  for (std::size_t slot = 0; slot < storage_.capacity(); ++slot) {
    storage_.eraseRecord(slot);
  }
  index_ = data::HistoryIndex{};
  persistIndex();
  healthy_ = true;
}

std::uint32_t RingHistoryStore::nextLocalRecordId() const noexcept {
  return index_.nextLocalRecordId;
}

}  // namespace jinggua::application
