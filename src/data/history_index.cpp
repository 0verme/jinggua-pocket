#include "jinggua/data/history_index.h"

#include <cstring>

#include "jinggua/domain/history_record.h"  // for crc16 helper

namespace jinggua::data {
namespace {

// Reuse the record CRC16 primitive so index and records share one checksum
// implementation.
std::uint16_t crc16(const std::uint8_t* data, std::size_t size) noexcept {
  return domain::historyRecordCrc16(data, size);
}

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

// Layout (20 bytes, little-endian):
//   0     magic 0x4A 'J'
//   1     schema_version (uint8)
//   2–5   head (uint32 LE)
//   6–9   count (uint32 LE)
//  10–13  next_local_record_id (uint32 LE)
//  14–17  next_session_order (uint32 LE)
//  18–19  crc16 (uint16 LE, over bytes 0–17)
constexpr std::uint8_t kMagic = 0x4A;
constexpr std::size_t kPayloadSize = 18;
constexpr std::size_t kCrcOffset = 18;

}  // namespace

std::size_t serializeHistoryIndex(const HistoryIndex& index,
                                  std::uint8_t* data,
                                  std::size_t capacity) noexcept {
  if (capacity < kHistoryIndexSerializedSize) {
    return 0;
  }
  std::memset(data, 0, kHistoryIndexSerializedSize);
  data[0] = kMagic;
  data[1] = index.schemaVersion;
  writeLe32(data + 2, index.head);
  writeLe32(data + 6, index.count);
  writeLe32(data + 10, index.nextLocalRecordId);
  writeLe32(data + 14, index.nextSessionOrder);

  const auto crc = crc16(data, kPayloadSize);
  data[kCrcOffset] = static_cast<std::uint8_t>(crc & 0xFFU);
  data[kCrcOffset + 1] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);
  return kHistoryIndexSerializedSize;
}

domain::HistoryLoadStatus loadHistoryIndex(const std::uint8_t* data,
                                           std::size_t size,
                                           HistoryIndex& out) noexcept {
  if (size < kHistoryIndexSerializedSize) {
    return domain::HistoryLoadStatus::TooShort;
  }
  if (data[0] != kMagic) {
    return domain::HistoryLoadStatus::CrcMismatch;
  }
  const auto storedCrc = static_cast<std::uint16_t>(data[kCrcOffset]) |
                         (static_cast<std::uint16_t>(data[kCrcOffset + 1])
                          << 8U);
  if (storedCrc != crc16(data, kPayloadSize)) {
    return domain::HistoryLoadStatus::CrcMismatch;
  }
  const auto version = data[1];
  if (version > domain::kHistorySchemaVersion) {
    return domain::HistoryLoadStatus::UnsupportedSchema;
  }

  HistoryIndex index;
  index.schemaVersion = version;
  index.head = readLe32(data + 2);
  index.count = readLe32(data + 6);
  index.nextLocalRecordId = readLe32(data + 10);
  index.nextSessionOrder = readLe32(data + 14);
  out = index;
  return domain::HistoryLoadStatus::Ok;
}

bool migrateHistoryIndex(HistoryIndex& index) noexcept {
  if (index.schemaVersion == domain::kHistorySchemaVersion) {
    return true;
  }
  if (index.schemaVersion == 0) {
    index.schemaVersion = domain::kHistorySchemaVersion;
    return true;
  }
  return false;
}

}  // namespace jinggua::data
