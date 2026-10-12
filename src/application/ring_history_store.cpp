#include "jinggua/application/ring_history_store.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace jinggua::application {
namespace {

constexpr std::uint8_t kRecoveryJournalMagic = 0x52U;
constexpr std::uint8_t kRecoveryJournalVersion = 1U;
constexpr std::size_t kRecoveryJournalHeaderSize = 4U;
constexpr std::size_t kRecoveryJournalCrcSize = 2U;
constexpr std::size_t kRecoveryJournalMaxSize =
    kRecoveryJournalHeaderSize +
    64U * domain::kHistoryRecordSerializedSize + kRecoveryJournalCrcSize;

std::size_t serializeRecoveryJournal(
    const std::array<domain::HistoryRecord, 64>& records, std::size_t count,
    std::uint8_t* data, std::size_t capacity) noexcept {
  if (count > records.size()) {
    return 0;
  }
  const auto size = kRecoveryJournalHeaderSize +
                    count * domain::kHistoryRecordSerializedSize +
                    kRecoveryJournalCrcSize;
  if (capacity < size) {
    return 0;
  }

  data[0] = kRecoveryJournalMagic;
  data[1] = kRecoveryJournalVersion;
  data[2] = static_cast<std::uint8_t>(count);
  data[3] = 0U;
  std::size_t offset = kRecoveryJournalHeaderSize;
  for (std::size_t i = 0; i < count; ++i) {
    const auto written = domain::serializeHistoryRecord(
        records[i], data + offset, domain::kHistoryRecordSerializedSize);
    if (written != domain::kHistoryRecordSerializedSize) {
      return 0;
    }
    offset += written;
  }

  const auto crc = domain::historyRecordCrc16(data, size - kRecoveryJournalCrcSize);
  data[size - 2U] = static_cast<std::uint8_t>(crc & 0xFFU);
  data[size - 1U] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);
  return size;
}

bool loadRecoveryJournal(
    const std::uint8_t* data, std::size_t size,
    std::array<domain::HistoryRecord, 64>& records,
    std::size_t& countOut) noexcept {
  if (size < kRecoveryJournalHeaderSize + kRecoveryJournalCrcSize ||
      data[0] != kRecoveryJournalMagic ||
      data[1] != kRecoveryJournalVersion || data[3] != 0U) {
    return false;
  }

  const auto count = static_cast<std::size_t>(data[2]);
  const auto expectedSize = kRecoveryJournalHeaderSize +
                            count * domain::kHistoryRecordSerializedSize +
                            kRecoveryJournalCrcSize;
  if (count > records.size() || size != expectedSize) {
    return false;
  }

  const auto storedCrc = static_cast<std::uint16_t>(data[size - 2U]) |
                         (static_cast<std::uint16_t>(data[size - 1U]) << 8U);
  if (storedCrc !=
      domain::historyRecordCrc16(data, size - kRecoveryJournalCrcSize)) {
    return false;
  }

  std::size_t offset = kRecoveryJournalHeaderSize;
  for (std::size_t i = 0; i < count; ++i) {
    if (domain::loadHistoryRecord(data + offset,
                                  domain::kHistoryRecordSerializedSize,
                                  records[i]) != domain::HistoryLoadStatus::Ok) {
      return false;
    }
    if (records[i].schemaVersion < domain::kHistorySchemaVersion &&
        !domain::migrateHistoryRecord(records[i])) {
      return false;
    }
    offset += domain::kHistoryRecordSerializedSize;
  }
  countOut = count;
  return true;
}

}  // namespace

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

RingHistoryStore::ActiveIndexCheckResult
RingHistoryStore::activeIndexMatchesRecords() const noexcept {
  const auto capacity = storage_.capacity();
  if (capacity == 0 || capacity > 64U || index_.count > capacity ||
      index_.head >= capacity) {
    return ActiveIndexCheckResult::Mismatch;
  }

  domain::HistoryRecord previous{};
  bool hasPrevious = false;
  for (std::size_t logical = 0; logical < index_.count; ++logical) {
    domain::HistoryRecord current{};
    const auto status = validateSlot(physicalSlot(logical), current);
    if (status == SlotValidationResult::Error) {
      return ActiveIndexCheckResult::Error;
    }
    if (status != SlotValidationResult::Valid) {
      return ActiveIndexCheckResult::Mismatch;
    }
    if (hasPrevious &&
        (current.sessionOrder < previous.sessionOrder ||
         (current.sessionOrder == previous.sessionOrder &&
          current.localRecordId < previous.localRecordId))) {
      return ActiveIndexCheckResult::Mismatch;
    }
    previous = current;
    hasPrevious = true;
  }

  std::array<std::uint8_t, domain::kHistoryRecordSerializedSize> blob{};
  for (std::size_t logical = index_.count; logical < capacity; ++logical) {
    std::size_t size = 0;
    const auto status = storage_.readRecord(
        physicalSlot(logical), blob.data(), blob.size(), size);
    if (status == RecordReadResult::Error) {
      return ActiveIndexCheckResult::Error;
    }
    if (status == RecordReadResult::Ready) {
      return ActiveIndexCheckResult::Mismatch;
    }
  }
  return ActiveIndexCheckResult::Match;
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
RingHistoryStore::SlotValidationResult RingHistoryStore::validateSlot(
    std::size_t slot, domain::HistoryRecord& out) const noexcept {
  std::array<std::uint8_t, domain::kHistoryRecordSerializedSize> blob{};
  std::size_t size = 0;
  const auto readResult =
      storage_.readRecord(slot, blob.data(), blob.size(), size);
  if (readResult == RecordReadResult::Missing) {
    return SlotValidationResult::Missing;
  }
  if (readResult == RecordReadResult::Error) {
    return SlotValidationResult::Error;
  }

  auto status = domain::loadHistoryRecord(blob.data(), size, out);
  if (status != domain::HistoryLoadStatus::Ok) {
    return SlotValidationResult::Corrupt;
  }
  // Migrate older schemas in memory. The upgraded bytes are only persisted
  // lazily, when this slot is next overwritten by add().
  if (out.schemaVersion < domain::kHistorySchemaVersion &&
      !domain::migrateHistoryRecord(out)) {
    return SlotValidationResult::Corrupt;
  }
  return SlotValidationResult::Valid;
}

// ---------------------------------------------------------------------------
// Recovery
// ---------------------------------------------------------------------------
bool RingHistoryStore::applyRecoveryJournal(
    const std::array<domain::HistoryRecord, 64>& records,
    std::size_t count) noexcept {
  if (count > storage_.capacity() || count > records.size()) {
    return false;
  }

  data::HistoryIndex rebuilt{};
  rebuilt.schemaVersion = domain::kHistorySchemaVersion;
  rebuilt.head = 0;
  rebuilt.count = static_cast<std::uint32_t>(count);
  for (std::size_t i = 0; i < count; ++i) {
    std::array<std::uint8_t, domain::kHistoryRecordSerializedSize> blob{};
    const auto size =
        domain::serializeHistoryRecord(records[i], blob.data(), blob.size());
    if (!storage_.writeRecord(i, blob.data(), size)) {
      return false;
    }
    if (records[i].localRecordId >= rebuilt.nextLocalRecordId) {
      rebuilt.nextLocalRecordId = records[i].localRecordId + 1;
    }
    if (records[i].sessionOrder >= rebuilt.nextSessionOrder) {
      rebuilt.nextSessionOrder = records[i].sessionOrder + 1;
    }
  }

  for (std::size_t slot = count; slot < storage_.capacity(); ++slot) {
    if (!storage_.eraseRecord(slot)) {
      return false;
    }
  }

  index_ = rebuilt;
  if (!persistIndex() || !storage_.eraseRecoveryJournal()) {
    return false;
  }
  healthy_ = true;
  return true;
}

bool RingHistoryStore::resumePendingRecoveryJournal(bool& handled) noexcept {
  handled = false;
  std::array<std::uint8_t, kRecoveryJournalMaxSize> journal{};
  std::size_t journalSize = 0;
  const auto status = storage_.readRecoveryJournal(
      journal.data(), journal.size(), journalSize);
  if (status == RecoveryJournalReadResult::Missing) {
    return true;
  }
  if (status == RecoveryJournalReadResult::Error) {
    return false;
  }

  handled = true;
  std::array<domain::HistoryRecord, 64> records{};
  std::size_t count = 0;
  if (!loadRecoveryJournal(journal.data(), journalSize, records, count)) {
    return false;
  }
  return applyRecoveryJournal(records, count);
}

bool RingHistoryStore::rebuildFromSlots() noexcept {
  // Keep the recovery buffer bounded even if a future backend exposes more
  // slots than the firmware's fixed history capacity.
  if (storage_.capacity() > 64U) {
    return false;
  }

  std::array<domain::HistoryRecord, 64> found{};
  std::size_t foundCount = 0;
  for (std::size_t slot = 0; slot < storage_.capacity(); ++slot) {
    domain::HistoryRecord record;
    const auto status = validateSlot(slot, record);
    if (status == SlotValidationResult::Error) {
      return false;
    }
    if (status == SlotValidationResult::Valid) {
      found[foundCount++] = record;
    }
  }

  // Sort by identity/order, then collapse duplicates left by an interrupted
  // recovery from an older firmware before staging the new transaction.
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
  std::size_t uniqueCount = 0;
  for (std::size_t i = 0; i < foundCount; ++i) {
    if (uniqueCount > 0 &&
        found[uniqueCount - 1].sessionOrder == found[i].sessionOrder &&
        found[uniqueCount - 1].localRecordId == found[i].localRecordId) {
      continue;
    }
    found[uniqueCount++] = found[i];
  }
  foundCount = uniqueCount;

  std::array<std::uint8_t, kRecoveryJournalMaxSize> journal{};
  const auto journalSize = serializeRecoveryJournal(
      found, foundCount, journal.data(), journal.size());
  if (journalSize == 0 ||
      !storage_.writeRecoveryJournal(journal.data(), journalSize)) {
    return false;
  }
  return applyRecoveryJournal(found, foundCount);
}

// ---------------------------------------------------------------------------
// HistoryStore
// ---------------------------------------------------------------------------
bool RingHistoryStore::begin() noexcept {
  healthy_ = false;
  index_ = data::HistoryIndex{};
  if (!storage_.begin() || storage_.capacity() == 0 ||
      storage_.capacity() > 64U) {
    return false;
  }

  bool journalHandled = false;
  if (!resumePendingRecoveryJournal(journalHandled)) {
    index_ = data::HistoryIndex{};
    return false;
  }
  if (journalHandled) {
    if (!healthy_) {
      index_ = data::HistoryIndex{};
    }
    return healthy_;
  }

  if (loadIndex()) {
    const auto indexStatus = activeIndexMatchesRecords();
    if (indexStatus == ActiveIndexCheckResult::Match) {
      healthy_ = true;
      return true;
    }
    if (indexStatus == ActiveIndexCheckResult::Error) {
      index_ = data::HistoryIndex{};
      return false;
    }
  }

  // Missing, corrupt, stale, or inconsistent index → stage and replay a full
  // scan rebuild. A fresh device gets an empty, valid index.
  if (!rebuildFromSlots()) {
    index_ = data::HistoryIndex{};
    return false;
  }
  return true;
}

bool RingHistoryStore::add(domain::HistoryRecord& record) noexcept {
  if (!healthy_ || storage_.capacity() == 0) {
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
  if (!healthy_ || index >= index_.count) {
    return false;
  }
  const auto slot = physicalSlot(index);
  const auto status = validateSlot(slot, out);
  if (status != SlotValidationResult::Valid) {
    // Invalid or unavailable slot data cannot be returned to the caller.
    return false;
  }
  return true;
}

bool RingHistoryStore::clear() noexcept {
  if (!healthy_) {
    return false;
  }

  std::array<domain::HistoryRecord, 64> emptyRecords{};
  std::array<std::uint8_t, kRecoveryJournalMaxSize> journal{};
  const auto journalSize = serializeRecoveryJournal(
      emptyRecords, 0, journal.data(), journal.size());
  if (journalSize == 0 ||
      !storage_.writeRecoveryJournal(journal.data(), journalSize)) {
    // A failed/ambiguous NVS commit must not allow additional writes until the
    // pending transaction state has been checked again by begin().
    index_ = data::HistoryIndex{};
    healthy_ = false;
    return false;
  }

  // Persist the clear intent before erasing any slot. If an erase or index
  // write fails, begin() will replay this zero-record journal instead of
  // reconstructing the old records as orphans.
  index_ = data::HistoryIndex{};
  healthy_ = false;
  return applyRecoveryJournal(emptyRecords, 0);
}

std::uint32_t RingHistoryStore::nextLocalRecordId() const noexcept {
  return index_.nextLocalRecordId;
}

}  // namespace jinggua::application
