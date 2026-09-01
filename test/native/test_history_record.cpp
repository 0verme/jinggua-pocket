#include "test_framework.h"
#include "test_support.h"

#include <cstring>

#include "jinggua/domain/history_record.h"

using jinggua::domain::HistoryRecord;
using jinggua::domain::HistoryLoadStatus;
using jinggua::domain::SyncStatus;
using jinggua::domain::kHistorySchemaVersion;
using jinggua::domain::kHistoryRecordSerializedSize;
using jinggua::domain::makeHistoryRecord;
using jinggua::domain::serializeHistoryRecord;
using jinggua::domain::loadHistoryRecord;
using jinggua::domain::migrateHistoryRecord;
using jinggua::domain::historyRecordCrc16;

void runHistoryRecordTests(TestRunner& runner) {
  // -----------------------------------------------------------------------
  // Round-trip: serialize a record, then load it back, compare all fields.
  // -----------------------------------------------------------------------
  {
    HistoryRecord record;
    record.localRecordId = 42;
    record.timestampEpoch = 1234567890;
    record.timestampValid = true;
    record.sessionOrder = 7;
    record.lineTotals = {6, 7, 8, 9, 7, 8};
    record.originalNumber = 1;
    record.movingMask = 0b001010;  // lines 1 and 3 (0-indexed)
    record.changedNumber = 2;
    record.syncStatus = SyncStatus::Pending;
    record.schemaVersion = kHistorySchemaVersion;

    std::uint8_t blob[kHistoryRecordSerializedSize]{};
    const auto written = serializeHistoryRecord(record, blob, sizeof(blob));
    EXPECT_EQ(runner, written, kHistoryRecordSerializedSize);

    HistoryRecord loaded{};
    const auto status = loadHistoryRecord(blob, sizeof(blob), loaded);
    EXPECT_EQ(runner, status, HistoryLoadStatus::Ok);

    EXPECT_EQ(runner, loaded.localRecordId, record.localRecordId);
    EXPECT_EQ(runner, loaded.timestampEpoch, record.timestampEpoch);
    EXPECT_EQ(runner, loaded.timestampValid, record.timestampValid);
    EXPECT_EQ(runner, loaded.sessionOrder, record.sessionOrder);
    for (std::size_t i = 0; i < 6; ++i) {
      EXPECT_EQ(runner, loaded.lineTotals[i], record.lineTotals[i]);
    }
    EXPECT_EQ(runner, loaded.originalNumber, record.originalNumber);
    EXPECT_EQ(runner, loaded.movingMask, record.movingMask);
    EXPECT_EQ(runner, loaded.changedNumber, record.changedNumber);
    EXPECT_EQ(runner, loaded.syncStatus, record.syncStatus);
    EXPECT_EQ(runner, loaded.schemaVersion, record.schemaVersion);
  }

  // -----------------------------------------------------------------------
  // Schema version 0 (old) loads successfully and can be migrated.
  // -----------------------------------------------------------------------
  {
    HistoryRecord record;
    record.localRecordId = 1;
    record.lineTotals = {7, 7, 7, 7, 7, 7};
    record.originalNumber = 1;
    record.schemaVersion = 0;

    std::uint8_t blob[kHistoryRecordSerializedSize]{};
    serializeHistoryRecord(record, blob, sizeof(blob));
    blob[0] = 0;  // overwrite schema version to 0 (v0 layout)

    // Recompute CRC with version=0.
    const auto crc = historyRecordCrc16(blob, 24);
    blob[24] = static_cast<std::uint8_t>(crc & 0xFFU);
    blob[25] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

    HistoryRecord loaded{};
    EXPECT_EQ(runner, loadHistoryRecord(blob, sizeof(blob), loaded),
              HistoryLoadStatus::Ok);
    EXPECT_EQ(runner, loaded.schemaVersion, static_cast<std::uint8_t>(0));
    EXPECT(runner, migrateHistoryRecord(loaded));
    EXPECT_EQ(runner, loaded.schemaVersion, kHistorySchemaVersion);
  }

  // -----------------------------------------------------------------------
  // Unknown newer schema version → UnsupportedSchema (no crash).
  // -----------------------------------------------------------------------
  {
    constexpr std::uint8_t kFutureVersion = 99;
    HistoryRecord record;
    record.localRecordId = 1;
    record.lineTotals = {7, 7, 7, 7, 7, 7};
    record.originalNumber = 1;
    record.schemaVersion = kFutureVersion;

    std::uint8_t blob[kHistoryRecordSerializedSize]{};
    serializeHistoryRecord(record, blob, sizeof(blob));
    blob[0] = kFutureVersion;
    const auto crc = historyRecordCrc16(blob, 24);
    blob[24] = static_cast<std::uint8_t>(crc & 0xFFU);
    blob[25] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

    HistoryRecord loaded{};
    EXPECT_EQ(runner, loadHistoryRecord(blob, sizeof(blob), loaded),
              HistoryLoadStatus::UnsupportedSchema);
  }

  // -----------------------------------------------------------------------
  // Corrupted CRC → CrcMismatch.
  // -----------------------------------------------------------------------
  {
    HistoryRecord record;
    record.localRecordId = 1;
    record.lineTotals = {7, 7, 7, 7, 7, 7};
    record.originalNumber = 1;

    std::uint8_t blob[kHistoryRecordSerializedSize]{};
    serializeHistoryRecord(record, blob, sizeof(blob));
    blob[24] ^= 0xFFU;  // corrupt CRC

    HistoryRecord loaded{};
    EXPECT_EQ(runner, loadHistoryRecord(blob, sizeof(blob), loaded),
              HistoryLoadStatus::CrcMismatch);
  }

  // -----------------------------------------------------------------------
  // Truncated blob → TooShort.
  // -----------------------------------------------------------------------
  {
    std::uint8_t blob[10]{};  // far too short
    HistoryRecord loaded{};
    EXPECT_EQ(runner, loadHistoryRecord(blob, sizeof(blob), loaded),
              HistoryLoadStatus::TooShort);
  }

  // -----------------------------------------------------------------------
  // Invalid field ranges (line total < 6) → CrcMismatch.
  // -----------------------------------------------------------------------
  {
    HistoryRecord record;
    record.localRecordId = 1;
    record.lineTotals = {5, 7, 7, 7, 7, 7};  // 5 is invalid
    record.originalNumber = 1;

    std::uint8_t blob[kHistoryRecordSerializedSize]{};
    serializeHistoryRecord(record, blob, sizeof(blob));
    // blob[0] contains version (valid), blob[1] flags, blob[14] has lineTotals[0] = 5
    // The CRC was computed over the valid payload, so we need to change the
    // byte AND recompute the CRC to make it "validly corrupted" → test
    // field-range validation.
    // Actually we want the CRC to be valid but the field-range check to fail.
    // Write invalid data, then recompute CRC.
    blob[14] = 5;
    const auto crc = historyRecordCrc16(blob, 24);
    blob[24] = static_cast<std::uint8_t>(crc & 0xFFU);
    blob[25] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

    HistoryRecord loaded{};
    EXPECT_EQ(runner, loadHistoryRecord(blob, sizeof(blob), loaded),
              HistoryLoadStatus::CrcMismatch);
  }

  // -----------------------------------------------------------------------
  // Invalid originalNumber (0) → CrcMismatch.
  // -----------------------------------------------------------------------
  {
    HistoryRecord record;
    record.localRecordId = 1;
    record.lineTotals = {7, 7, 7, 7, 7, 7};
    record.originalNumber = 0;  // invalid

    std::uint8_t blob[kHistoryRecordSerializedSize]{};
    serializeHistoryRecord(record, blob, sizeof(blob));
    // Overwrite and recompute CRC so the payload is "valid" but field fails.
    blob[20] = 0;
    const auto crc = historyRecordCrc16(blob, 24);
    blob[24] = static_cast<std::uint8_t>(crc & 0xFFU);
    blob[25] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

    HistoryRecord loaded{};
    EXPECT_EQ(runner, loadHistoryRecord(blob, sizeof(blob), loaded),
              HistoryLoadStatus::CrcMismatch);
  }

  // -----------------------------------------------------------------------
  // makeHistoryRecord from a valid DivinationResult.
  // -----------------------------------------------------------------------
  {
    // Build a 6-line result with known moving lines.
    std::array<jinggua::domain::Yao, 6> lines = {
        yaoAt(1, 6),  // 老阴 → moving
        yaoAt(2, 7),  // 少阳
        yaoAt(3, 8),  // 少阴
        yaoAt(4, 9),  // 老阳 → moving
        yaoAt(5, 7),  // 少阳
        yaoAt(6, 8),  // 少阴
    };
    const auto result = jinggua::domain::createDivinationResult(lines);
    EXPECT(runner, result.has_value());
    EXPECT(runner, result->hasMovingLines());

    const auto record = makeHistoryRecord(*result);
    EXPECT(runner, record.has_value());

    // Check derived fields.
    EXPECT_EQ(runner, record->originalNumber,
              result->original.number);
    EXPECT_EQ(runner, record->movingMask,
              static_cast<std::uint8_t>(0b001001));  // lines 1 and 4
    EXPECT(runner, record->changedNumber != 0);
    EXPECT_EQ(runner, record->lineTotals[0], static_cast<std::uint8_t>(6));
    EXPECT_EQ(runner, record->lineTotals[3], static_cast<std::uint8_t>(9));
    EXPECT_EQ(runner, record->syncStatus, SyncStatus::Pending);
    EXPECT_EQ(runner, record->schemaVersion, kHistorySchemaVersion);
  }

  // -----------------------------------------------------------------------
  // makeHistoryRecord from a result without moving lines.
  // -----------------------------------------------------------------------
  {
    std::array<jinggua::domain::Yao, 6> lines = {
        yaoAt(1, 7), yaoAt(2, 7), yaoAt(3, 7),
        yaoAt(4, 7), yaoAt(5, 7), yaoAt(6, 7),
    };
    const auto result = jinggua::domain::createDivinationResult(lines);
    EXPECT(runner, result.has_value());
    EXPECT(runner, !result->hasMovingLines());

    const auto record = makeHistoryRecord(*result);
    EXPECT(runner, record.has_value());
    EXPECT_EQ(runner, record->changedNumber,
              static_cast<std::uint8_t>(0));
    EXPECT_EQ(runner, record->movingMask,
              static_cast<std::uint8_t>(0));
  }

  // -----------------------------------------------------------------------
  // CRC consistency: same input produces same output.
  // -----------------------------------------------------------------------
  {
    const std::uint8_t data[] = {1, 2, 3, 4, 5};
    const auto crc1 = historyRecordCrc16(data, sizeof(data));
    const auto crc2 = historyRecordCrc16(data, sizeof(data));
    EXPECT_EQ(runner, crc1, crc2);
  }
}
