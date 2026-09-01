#include "test_framework.h"
#include "test_support.h"

#include "jinggua/data/history_index.h"
#include "jinggua/domain/history_record.h"

using jinggua::data::HistoryIndex;
using jinggua::data::kHistoryIndexSerializedSize;
using jinggua::data::serializeHistoryIndex;
using jinggua::data::loadHistoryIndex;
using jinggua::data::migrateHistoryIndex;
using jinggua::domain::HistoryLoadStatus;
using jinggua::domain::kHistorySchemaVersion;

void runHistoryIndexTests(TestRunner& runner) {
  // -----------------------------------------------------------------------
  // Round-trip serialize → load → equal.
  // -----------------------------------------------------------------------
  {
    HistoryIndex index;
    index.schemaVersion = kHistorySchemaVersion;
    index.head = 3;
    index.count = 12;
    index.nextLocalRecordId = 42;
    index.nextSessionOrder = 99;

    std::uint8_t blob[kHistoryIndexSerializedSize]{};
    const auto written = serializeHistoryIndex(index, blob, sizeof(blob));
    EXPECT_EQ(runner, written, kHistoryIndexSerializedSize);

    HistoryIndex loaded{};
    EXPECT_EQ(runner, loadHistoryIndex(blob, sizeof(blob), loaded),
              HistoryLoadStatus::Ok);
    EXPECT_EQ(runner, loaded.schemaVersion, index.schemaVersion);
    EXPECT_EQ(runner, loaded.head, index.head);
    EXPECT_EQ(runner, loaded.count, index.count);
    EXPECT_EQ(runner, loaded.nextLocalRecordId, index.nextLocalRecordId);
    EXPECT_EQ(runner, loaded.nextSessionOrder, index.nextSessionOrder);
  }

  // -----------------------------------------------------------------------
  // Corrupted CRC → CrcMismatch.
  // -----------------------------------------------------------------------
  {
    HistoryIndex index;
    index.count = 5;
    std::uint8_t blob[kHistoryIndexSerializedSize]{};
    serializeHistoryIndex(index, blob, sizeof(blob));
    blob[18] ^= 0xFFU;  // corrupt CRC

    HistoryIndex loaded{};
    EXPECT_EQ(runner, loadHistoryIndex(blob, sizeof(blob), loaded),
              HistoryLoadStatus::CrcMismatch);
  }

  // -----------------------------------------------------------------------
  // Too short → TooShort.
  // -----------------------------------------------------------------------
  {
    std::uint8_t blob[8]{};
    HistoryIndex loaded{};
    EXPECT_EQ(runner, loadHistoryIndex(blob, sizeof(blob), loaded),
              HistoryLoadStatus::TooShort);
  }

  // -----------------------------------------------------------------------
  // Bad magic → CrcMismatch.
  // -----------------------------------------------------------------------
  {
    HistoryIndex index;
    std::uint8_t blob[kHistoryIndexSerializedSize]{};
    serializeHistoryIndex(index, blob, sizeof(blob));
    blob[0] = 0x00;  // invalid magic
    const auto crc =
        jinggua::domain::historyRecordCrc16(blob, 18);
    blob[18] = static_cast<std::uint8_t>(crc & 0xFFU);
    blob[19] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

    HistoryIndex loaded{};
    EXPECT_EQ(runner, loadHistoryIndex(blob, sizeof(blob), loaded),
              HistoryLoadStatus::CrcMismatch);
  }

  // -----------------------------------------------------------------------
  // Newer schema version → UnsupportedSchema.
  // -----------------------------------------------------------------------
  {
    HistoryIndex index;
    index.schemaVersion = static_cast<std::uint8_t>(99);
    std::uint8_t blob[kHistoryIndexSerializedSize]{};
    serializeHistoryIndex(index, blob, sizeof(blob));
    const auto crc = jinggua::domain::historyRecordCrc16(blob, 18);
    blob[18] = static_cast<std::uint8_t>(crc & 0xFFU);
    blob[19] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

    HistoryIndex loaded{};
    EXPECT_EQ(runner, loadHistoryIndex(blob, sizeof(blob), loaded),
              HistoryLoadStatus::UnsupportedSchema);
  }

  // -----------------------------------------------------------------------
  // Migration v0 → v1.
  // -----------------------------------------------------------------------
  {
    HistoryIndex index;
    index.schemaVersion = 0;
    EXPECT(runner, migrateHistoryIndex(index));
    EXPECT_EQ(runner, index.schemaVersion, kHistorySchemaVersion);
  }
}
