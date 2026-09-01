#include "test_framework.h"
#include "test_support.h"

#include "jinggua/application/ring_history_store.h"
#include "jinggua/domain/history_record.h"

using jinggua::application::RingHistoryStore;
using jinggua::domain::HistoryRecord;
using jinggua::domain::HistoryLoadStatus;
using jinggua::domain::kHistorySchemaVersion;
using jinggua::domain::kHistoryRecordSerializedSize;
using jinggua::domain::makeHistoryRecord;
using jinggua::domain::serializeHistoryRecord;
using jinggua::domain::historyRecordCrc16;

// Helper: build a simple record with a known identity.
HistoryRecord makeSimpleRecord(std::uint32_t id, std::uint8_t originalNumber,
                               std::uint8_t lineTotal) noexcept {
  HistoryRecord rec;
  rec.localRecordId = id;
  rec.sessionOrder = id;
  rec.originalNumber = originalNumber;
  rec.lineTotals = {lineTotal, lineTotal, lineTotal,
                    lineTotal, lineTotal, lineTotal};
  rec.schemaVersion = kHistorySchemaVersion;
  return rec;
}

void runRingHistoryStoreTests(TestRunner& runner) {
  // -----------------------------------------------------------------------
  // Empty store: count=0, get returns false, nextLocalRecordId=1.
  // -----------------------------------------------------------------------
  {
    InMemorySlotStorage storage(4);
    RingHistoryStore store(storage);
    EXPECT(runner, store.begin());
    EXPECT_EQ(runner, store.count(), static_cast<std::size_t>(0));
    EXPECT_EQ(runner, store.nextLocalRecordId(),
              static_cast<std::uint32_t>(1));

    HistoryRecord rec;
    EXPECT(runner, !store.get(0, rec));
  }

  // -----------------------------------------------------------------------
  // Add one record → count=1, get(0) matches, nextLocalRecordId advances.
  // -----------------------------------------------------------------------
  {
    InMemorySlotStorage storage(4);
    RingHistoryStore store(storage);
    EXPECT(runner, store.begin());

    auto rec = makeSimpleRecord(0, 1, 7);
    EXPECT(runner, store.add(rec));
    EXPECT_EQ(runner, rec.localRecordId, static_cast<std::uint32_t>(1));
    EXPECT_EQ(runner, rec.sessionOrder, static_cast<std::uint32_t>(1));

    EXPECT_EQ(runner, store.count(), static_cast<std::size_t>(1));
    EXPECT_EQ(runner, store.nextLocalRecordId(),
              static_cast<std::uint32_t>(2));

    HistoryRecord loaded;
    EXPECT(runner, store.get(0, loaded));
    EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(1));
    EXPECT_EQ(runner, loaded.originalNumber, static_cast<std::uint8_t>(1));
  }

  // -----------------------------------------------------------------------
  // Add N records, fill exactly to capacity.
  // -----------------------------------------------------------------------
  {
    constexpr std::size_t kCap = 5;
    InMemorySlotStorage storage(kCap);
    RingHistoryStore store(storage);
    EXPECT(runner, store.begin());

    for (std::uint32_t i = 1; i <= kCap; ++i) {
      auto rec = makeSimpleRecord(i, static_cast<std::uint8_t>(i), 7);
      EXPECT(runner, store.add(rec));
    }
    EXPECT_EQ(runner, store.count(), kCap);

    // Verify each record in order.
    for (std::size_t i = 0; i < kCap; ++i) {
      HistoryRecord loaded;
      EXPECT(runner, store.get(i, loaded));
      EXPECT_EQ(runner, loaded.localRecordId,
                static_cast<std::uint32_t>(i + 1));
      EXPECT_EQ(runner, loaded.originalNumber,
                static_cast<std::uint8_t>(i + 1));
    }
  }

  // -----------------------------------------------------------------------
  // Ring buffer eviction: add N+1 records, oldest is evicted.
  // -----------------------------------------------------------------------
  {
    constexpr std::size_t kCap = 3;
    InMemorySlotStorage storage(kCap);
    RingHistoryStore store(storage);
    EXPECT(runner, store.begin());

    for (std::uint32_t i = 1; i <= kCap + 1; ++i) {
      auto rec = makeSimpleRecord(i, static_cast<std::uint8_t>(i), 7);
      EXPECT(runner, store.add(rec));
    }
    EXPECT_EQ(runner, store.count(), kCap);

    // Record 1 (id=1) should be evicted; record 2..4 are present.
    HistoryRecord loaded;
    EXPECT(runner, store.get(0, loaded));
    EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(2));

    EXPECT(runner, store.get(1, loaded));
    EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(3));

    EXPECT(runner, store.get(2, loaded));
    EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(4));
  }

  // -----------------------------------------------------------------------
  // Save → begin() again → load → equal (persistence round-trip).
  // -----------------------------------------------------------------------
  {
    constexpr std::size_t kCap = 4;
    InMemorySlotStorage storage(kCap);
    {
      RingHistoryStore store(storage);
      EXPECT(runner, store.begin());

      for (std::uint32_t i = 1; i <= 3; ++i) {
        auto rec = makeSimpleRecord(i, static_cast<std::uint8_t>(i), 7);
        EXPECT(runner, store.add(rec));
      }
    }
    // Simulate a reboot by constructing a new store over the same storage.
    {
      RingHistoryStore store(storage);
      EXPECT(runner, store.begin());
      EXPECT_EQ(runner, store.count(), static_cast<std::size_t>(3));

      HistoryRecord loaded;
      EXPECT(runner, store.get(0, loaded));
      EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(1));

      EXPECT(runner, store.get(1, loaded));
      EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(2));

      EXPECT(runner, store.get(2, loaded));
      EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(3));
    }
  }

  // -----------------------------------------------------------------------
  // Corrupted index meta → rebuild from slots, records preserved.
  // -----------------------------------------------------------------------
  {
    constexpr std::size_t kCap = 4;
    InMemorySlotStorage storage(kCap);
    // Populate a store.
    {
      RingHistoryStore store(storage);
      EXPECT(runner, store.begin());
      for (std::uint32_t i = 1; i <= 3; ++i) {
        auto rec = makeSimpleRecord(i, static_cast<std::uint8_t>(i), 7);
        EXPECT(runner, store.add(rec));
      }
    }
    // Corrupt the index.
    storage.corruptIndexPayload();

    // Reboot: index is corrupt, must rebuild from slots.
    {
      RingHistoryStore store(storage);
      EXPECT(runner, store.begin());
      EXPECT_EQ(runner, store.count(), static_cast<std::size_t>(3));

      // Records should be recoverable (sorted by sessionOrder).
      HistoryRecord loaded;
      EXPECT(runner, store.get(0, loaded));
      EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(1));
      EXPECT(runner, store.get(1, loaded));
      EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(2));
      EXPECT(runner, store.get(2, loaded));
      EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(3));

      // nextLocalRecordId should continue from max+1 = 4.
      EXPECT_EQ(runner, store.nextLocalRecordId(),
                static_cast<std::uint32_t>(4));
    }
  }

  // -----------------------------------------------------------------------
  // Corrupted single record → other records are still accessible.
  // -----------------------------------------------------------------------
  {
    constexpr std::size_t kCap = 4;
    InMemorySlotStorage storage(kCap);
    RingHistoryStore store(storage);
    EXPECT(runner, store.begin());

    for (std::uint32_t i = 1; i <= kCap; ++i) {
      auto rec = makeSimpleRecord(i, static_cast<std::uint8_t>(i), 7);
      EXPECT(runner, store.add(rec));
    }

    // Corrupt the second record (slot 1).
    storage.corruptRecordPayload(1);

    // First and third records should still be accessible.
    HistoryRecord loaded;
    EXPECT(runner, store.get(0, loaded));
    EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(1));

    // The corrupted slot returns false.
    EXPECT(runner, !store.get(1, loaded));

    // Records after the corrupted one shift by one in logical index.
    EXPECT(runner, store.get(2, loaded));
    EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(3));

    EXPECT(runner, store.get(3, loaded));
    EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(4));
  }

  // -----------------------------------------------------------------------
  // Clear → count=0, get returns false, nextLocalRecordId resets.
  // -----------------------------------------------------------------------
  {
    constexpr std::size_t kCap = 4;
    InMemorySlotStorage storage(kCap);
    RingHistoryStore store(storage);
    EXPECT(runner, store.begin());

    auto rec = makeSimpleRecord(0, 1, 7);
    EXPECT(runner, store.add(rec));
    EXPECT_EQ(runner, store.count(), static_cast<std::size_t>(1));

    store.clear();
    EXPECT_EQ(runner, store.count(), static_cast<std::size_t>(0));
    EXPECT_EQ(runner, store.nextLocalRecordId(),
              static_cast<std::uint32_t>(1));

    HistoryRecord loaded;
    EXPECT(runner, !store.get(0, loaded));
  }

  // -----------------------------------------------------------------------
  // nextLocalRecordId is monotonic across multiple adds.
  // -----------------------------------------------------------------------
  {
    InMemorySlotStorage storage(5);
    RingHistoryStore store(storage);
    EXPECT(runner, store.begin());
    EXPECT_EQ(runner, store.nextLocalRecordId(),
              static_cast<std::uint32_t>(1));

    for (std::uint32_t i = 0; i < 3; ++i) {
      auto rec = makeSimpleRecord(0, 1, 7);
      EXPECT(runner, store.add(rec));
    }
    EXPECT_EQ(runner, store.nextLocalRecordId(),
              static_cast<std::uint32_t>(4));
  }

  // -----------------------------------------------------------------------
  // Old schema (v0) record survives rebuild and is migrated.
  // -----------------------------------------------------------------------
  {
    constexpr std::size_t kCap = 4;
    InMemorySlotStorage storage(kCap);
    // Manually write a v0 record blob directly into slot 0, then corrupt
    // the index to force a rebuild.
    {
      HistoryRecord rec;
      rec.localRecordId = 1;
      rec.sessionOrder = 1;
      rec.originalNumber = 10;
      rec.lineTotals = {7, 7, 7, 7, 7, 7};
      rec.schemaVersion = 0;  // old schema

      std::uint8_t blob[kHistoryRecordSerializedSize]{};
      auto size = serializeHistoryRecord(rec, blob, sizeof(blob));
      blob[0] = 0;  // set version to 0
      const auto crc = historyRecordCrc16(blob, 24);
      blob[24] = static_cast<std::uint8_t>(crc & 0xFFU);
      blob[25] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

      storage.writeRecord(0, blob, size);
    }

    // Rebuild (index is missing → no index, so begin will rebuild).
    {
      RingHistoryStore store(storage);
      EXPECT(runner, store.begin());
      EXPECT_EQ(runner, store.count(), static_cast<std::size_t>(1));

      HistoryRecord loaded;
      EXPECT(runner, store.get(0, loaded));
      // The record should have been migrated to current schema.
      EXPECT_EQ(runner, loaded.schemaVersion, kHistorySchemaVersion);
      EXPECT_EQ(runner, loaded.originalNumber, static_cast<std::uint8_t>(10));
      EXPECT_EQ(runner, loaded.localRecordId, static_cast<std::uint32_t>(1));
    }
  }
}
