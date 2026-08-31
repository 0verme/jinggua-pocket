#include "jinggua/domain/history_record.h"

#include <cstdint>
#include <cstring>

namespace jinggua::domain {
namespace {

// ---------------------------------------------------------------------------
// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no final xor).
// Bit-by-bit implementation, no lookup table, to keep the code trivial on
// every ESP32 and native toolchain.
// ---------------------------------------------------------------------------
std::uint16_t crc16Ccitt(const std::uint8_t* data, std::size_t size) noexcept {
  std::uint16_t crc = 0xFFFFU;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= static_cast<std::uint16_t>(data[i]) << 8U;
    for (int bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000U) {
        crc = static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U);
      } else {
        crc = static_cast<std::uint16_t>(crc << 1U);
      }
    }
  }
  return crc;
}

// ---------------------------------------------------------------------------
// Binary layout (26 bytes, little-endian):
//
//   0   schema_version   (uint8)
//   1   flags            (uint8): bit0 = timestampValid
//   2–5  local_record_id  (uint32 LE)
//   6–9  timestamp_epoch  (uint32 LE)
//  10–13 session_order    (uint32 LE)
//  14–19 line_totals[6]   (6 × uint8)
//  20   original_number   (uint8)
//  21   moving_mask       (uint8)
//  22   changed_number    (uint8)
//  23   sync_status       (uint8)
//  24–25 crc16            (uint16 LE, computed over bytes 0–23)
// ---------------------------------------------------------------------------

constexpr std::size_t kPayloadSize = 24;  // bytes 0–23
constexpr std::size_t kCrcOffset = 24;    // bytes 24–25

void writeLe32(std::uint8_t* dst, std::uint32_t value) noexcept {
  dst[0] = static_cast<std::uint8_t>(value & 0xFFU);
  dst[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
  dst[2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
  dst[3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

std::uint32_t readLe32(const std::uint8_t* src) noexcept {
  return static_cast<std::uint32_t>(src[0]) |
         (static_cast<std::uint32_t>(src[1]) << 8U) |
         (static_cast<std::uint32_t>(src[2]) << 16U) |
         (static_cast<std::uint32_t>(src[3]) << 24U);
}

}  // namespace

// ---------------------------------------------------------------------------
// makeHistoryRecord
// ---------------------------------------------------------------------------
std::optional<HistoryRecord> makeHistoryRecord(
    const DivinationResult& result) noexcept {
  // Guard: the result must have a valid original hexagram.
  if (result.original.number == 0 || result.original.name == nullptr) {
    return std::nullopt;
  }

  HistoryRecord rec;
  rec.originalNumber = result.original.number;
  rec.movingMask = 0;
  for (std::uint8_t i = 0; i < result.movingCount; ++i) {
    const auto pos = result.movingPositions[i];
    if (pos >= 1 && pos <= 6) {
      rec.movingMask |= static_cast<std::uint8_t>(1U << (pos - 1U));
    }
  }
  if (result.transformed.has_value()) {
    rec.changedNumber = result.transformed->number;
  } else {
    rec.changedNumber = 0;
  }

  // Extract line totals from the original hexagram's lines.
  for (std::size_t i = 0; i < 6; ++i) {
    rec.lineTotals[i] = result.original.lines[i].coins.total;
  }

  return rec;
}

// ---------------------------------------------------------------------------
// serializeHistoryRecord
// ---------------------------------------------------------------------------
std::size_t serializeHistoryRecord(const HistoryRecord& record,
                                   std::uint8_t* data,
                                   std::size_t capacity) noexcept {
  if (capacity < kHistoryRecordSerializedSize) {
    return 0;
  }

  std::memset(data, 0, kHistoryRecordSerializedSize);

  data[0] = record.schemaVersion;
  data[1] = record.timestampValid ? 0x01U : 0x00U;
  writeLe32(data + 2, record.localRecordId);
  writeLe32(data + 6, record.timestampEpoch);
  writeLe32(data + 10, record.sessionOrder);
  for (std::size_t i = 0; i < 6; ++i) {
    data[14 + i] = record.lineTotals[i];
  }
  data[20] = record.originalNumber;
  data[21] = record.movingMask;
  data[22] = record.changedNumber;
  data[23] = static_cast<std::uint8_t>(record.syncStatus);

  // CRC-16 over bytes 0–23, stored little-endian at bytes 24–25.
  const auto crc = crc16Ccitt(data, kPayloadSize);
  data[kCrcOffset] = static_cast<std::uint8_t>(crc & 0xFFU);
  data[kCrcOffset + 1] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

  return kHistoryRecordSerializedSize;
}

// ---------------------------------------------------------------------------
// loadHistoryRecord
// ---------------------------------------------------------------------------
HistoryLoadStatus loadHistoryRecord(const std::uint8_t* data,
                                    std::size_t size,
                                    HistoryRecord& out) noexcept {
  if (size < kHistoryRecordSerializedSize) {
    return HistoryLoadStatus::TooShort;
  }

  // Validate CRC before touching any field.
  const auto storedCrc =
      static_cast<std::uint16_t>(data[kCrcOffset]) |
      (static_cast<std::uint16_t>(data[kCrcOffset + 1]) << 8U);
  const auto computedCrc = crc16Ccitt(data, kPayloadSize);
  if (storedCrc != computedCrc) {
    return HistoryLoadStatus::CrcMismatch;
  }

  // Schema version: refuse unknown newer versions, tolerate older ones
  // (they will be upgraded by the caller via migrateHistoryRecord).
  const auto version = data[0];
  if (version > kHistorySchemaVersion) {
    return HistoryLoadStatus::UnsupportedSchema;
  }

  // Parse fields.
  HistoryRecord rec;
  rec.schemaVersion = version;
  rec.timestampValid = (data[1] & 0x01U) != 0;
  rec.localRecordId = readLe32(data + 2);
  rec.timestampEpoch = readLe32(data + 6);
  rec.sessionOrder = readLe32(data + 10);
  for (std::size_t i = 0; i < 6; ++i) {
    rec.lineTotals[i] = data[14 + i];
  }
  rec.originalNumber = data[20];
  rec.movingMask = data[21];
  rec.changedNumber = data[22];
  rec.syncStatus = static_cast<SyncStatus>(data[23]);

  // Validate field ranges. A corrupted record that passes CRC is extremely
  // unlikely, but a broken flash read could produce a valid CRC on garbage.
  for (std::size_t i = 0; i < 6; ++i) {
    const auto t = rec.lineTotals[i];
    if (t < 6 || t > 9) {
      return HistoryLoadStatus::CrcMismatch;  // treat as corruption
    }
  }
  if (rec.originalNumber < 1 || rec.originalNumber > 64) {
    return HistoryLoadStatus::CrcMismatch;
  }
  if (rec.changedNumber > 64) {
    return HistoryLoadStatus::CrcMismatch;
  }

  out = rec;
  return HistoryLoadStatus::Ok;
}

// ---------------------------------------------------------------------------
// migrateHistoryRecord
// ---------------------------------------------------------------------------
bool migrateHistoryRecord(HistoryRecord& record) noexcept {
  if (record.schemaVersion == kHistorySchemaVersion) {
    return true;  // already current, nothing to do
  }
  // v0 (development layout) → v1: the binary layout is identical, only the
  // version byte changes.  Future migrations would go here.
  if (record.schemaVersion == 0) {
    record.schemaVersion = kHistorySchemaVersion;
    return true;
  }
  // No known migration path for entries of a newer version; the caller
  // should have already rejected them via UnsupportedSchema.
  return false;
}

// ---------------------------------------------------------------------------
// historyRecordCrc16
// ---------------------------------------------------------------------------
std::uint16_t historyRecordCrc16(const std::uint8_t* data,
                                 std::size_t size) noexcept {
  return crc16Ccitt(data, size);
}

}  // namespace jinggua::domain