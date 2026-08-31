#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "jinggua/domain/divination.h"

namespace jinggua::domain {

// Current persisted history schema. Bump this whenever the binary layout
// changes; loadHistoryRecord() refuses anything newer than the current
// version instead of crashing, and migrateHistoryRecord() upgrades older
// versions when a migration path exists.
constexpr std::uint8_t kHistorySchemaVersion = 1;

// Serialized size of a history record in bytes (fixed-size binary layout so
// storage slots are directly indexable).
constexpr std::size_t kHistoryRecordSerializedSize = 26;

// Sync status is intentionally part of every record from day one even though
// cloud sync (#12) is out of scope for this issue: records written today are
// already carrying the field the future sync layer will flip to Synced.
enum class SyncStatus : std::uint8_t {
  Pending = 0,  // written locally, not yet synced anywhere
  Synced = 1,   // acknowledged by a future sync backend
  Failed = 2,   // a future sync attempt failed and may be retried
};

// A single completed divination (six lines) persisted on the device.
//
// Timestamp handling: the StickS3 has no battery-backed RTC and this build
// does not fetch network time, so timestampValid is usually false and
// timestampEpoch stays 0. Records must still be saveable and orderable
// without a clock: sessionOrder is a monotonic counter allocated by the
// store, and the epoch field is reserved so a future RTC/NTP-backed build
// can backfill real time without a schema change.
struct HistoryRecord {
  // Monotonic local id allocated by the store; stable across reboots.
  // Reserved as the natural key for future sync (#12).
  std::uint32_t localRecordId{0};

  // Epoch seconds when the divination completed. 0 when no valid clock is
  // available (see timestampValid).
  std::uint32_t timestampEpoch{0};

  // True when timestampEpoch came from a valid clock; false on this build
  // (no RTC / no network time), which UI renders as "记录 N" instead of a
  // wall-clock date.
  bool timestampValid{false};

  // Monotonic ordering counter allocated by the store. Never goes
  // backwards even if the clock jumps, so history is always orderable
  // offline.
  std::uint32_t sessionOrder{0};

  // One coin total per line, bottom-to-top (line 0 = 初爻). Each value is
  // 6..9 (6=老阴, 7=少阳, 8=少阴, 9=老阳); the three coins that produced it
  // are uniquely reconstructible from the total, so this fully determines
  // the Yao sequence and therefore both hexagrams.
  std::array<std::uint8_t, 6> lineTotals{};

  // King Wen number of the original hexagram (1..64).
  std::uint8_t originalNumber{0};

  // Bit i set when line i (0 = 初爻) is a moving line. Mirrors
  // DivinationResult::movingPositions for cheap scan/display.
  std::uint8_t movingMask{0};

  // King Wen number of the changed hexagram (1..64), or 0 when there are
  // no moving lines (the divination has no 之卦).
  std::uint8_t changedNumber{0};

  // Sync bookkeeping reserved for #12. Always Pending on this build.
  SyncStatus syncStatus{SyncStatus::Pending};

  // Schema of this record; must be kHistorySchemaVersion for new records.
  std::uint8_t schemaVersion{kHistorySchemaVersion};
};

// Result of parsing a serialized record; lets callers distinguish "skip this
// corrupted entry" from "this entry is from a newer firmware" so one bad
// record never takes down the whole history.
enum class HistoryLoadStatus {
  Ok,
  TooShort,
  UnsupportedSchema,  // newer schema than this firmware understands
  CrcMismatch,        // corrupted or partially written
};

// Builds a record from a completed divination. lineTotals/originalNumber/
// movingMask/changedNumber are derived from `result`; the caller supplies
// identity/ordering fields. Returns nullopt when the result is not a valid
// completed hexagram.
std::optional<HistoryRecord> makeHistoryRecord(
    const DivinationResult& result) noexcept;

// Serializes `record` into a fixed kHistoryRecordSerializedSize-byte blob
// (little-endian, trailing CRC-16/CCITT). Returns the number of bytes
// written, or 0 when the buffer is too small.
std::size_t serializeHistoryRecord(const HistoryRecord& record,
                                   std::uint8_t* data,
                                   std::size_t capacity) noexcept;

// Parses a blob produced by serializeHistoryRecord. On Ok the record is
// fully validated (size, schema, CRC, field ranges). Never throws and never
// crashes on garbage input.
HistoryLoadStatus loadHistoryRecord(const std::uint8_t* data,
                                    std::size_t size,
                                    HistoryRecord& out) noexcept;

// Upgrades an older record to the current schema in place. The first
// released schema is v1; v0 was the development-era layout (same fields,
// version byte only), so this is currently a no-op that stamps the current
// version. Returns false when no migration path exists (caller should drop
// the record rather than crash).
bool migrateHistoryRecord(HistoryRecord& record) noexcept;

// CRC-16/CCITT-FALSE over the record's bytes. Exposed for tests and for
// future consumers that re-checksum whole blobs.
std::uint16_t historyRecordCrc16(const std::uint8_t* data,
                                 std::size_t size) noexcept;

}  // namespace jinggua::domain
