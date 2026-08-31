#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include <utility>
#include <vector>

#include "jinggua/application/random_provider.h"
#include "jinggua/application/slot_storage.h"
#include "jinggua/domain/history_record.h"
#include "jinggua/domain/yao.h"

class SequenceRandomProvider final : public jinggua::application::RandomProvider {
 public:
  explicit SequenceRandomProvider(std::vector<jinggua::domain::CoinSide> sequence)
      : sequence_(std::move(sequence)) {}

  jinggua::domain::CoinSide tossCoin() override {
    if (index_ >= sequence_.size()) {
      return jinggua::domain::CoinSide::Back;
    }
    return sequence_[index_++];
  }

  std::size_t consumed() const { return index_; }

 private:
  std::vector<jinggua::domain::CoinSide> sequence_;
  std::size_t index_{0};
};

inline jinggua::domain::CoinResult coinsForTotal(std::uint8_t total) {
  using jinggua::domain::CoinSide;
  if (total == 6) {
    return jinggua::domain::CoinResult::fromCoins(
        {CoinSide::Back, CoinSide::Back, CoinSide::Back});
  }
  if (total == 7) {
    return jinggua::domain::CoinResult::fromCoins(
        {CoinSide::Front, CoinSide::Back, CoinSide::Back});
  }
  if (total == 8) {
    return jinggua::domain::CoinResult::fromCoins(
        {CoinSide::Front, CoinSide::Front, CoinSide::Back});
  }
  if (total == 9) {
    return jinggua::domain::CoinResult::fromCoins(
        {CoinSide::Front, CoinSide::Front, CoinSide::Front});
  }
  assert(false && "total must be between 6 and 9");
  return {};
}

inline jinggua::domain::Yao yaoAt(std::uint8_t position,
                                  std::uint8_t total) {
  const auto yao = jinggua::domain::createYao(position, coinsForTotal(total));
  assert(yao.has_value());
  return *yao;
}

// In-memory SlotStorage test double: mimics the NVS backend's semantics
// (fixed-size slots, empty slots read as absent) so RingHistoryStore can be
// exercised natively without hardware. Also supports intentionally corrupted
// bytes for recovery tests.
class InMemorySlotStorage final : public jinggua::application::SlotStorage {
 public:
  explicit InMemorySlotStorage(std::size_t capacity) noexcept
      : records_(capacity), sizes_(capacity, 0) {}

  std::size_t capacity() const noexcept override { return records_.size(); }

  bool readRecord(std::size_t slot, std::uint8_t* data, std::size_t capacity,
                  std::size_t& sizeOut) const noexcept override {
    if (slot >= records_.size() || sizes_[slot] == 0) {
      return false;
    }
    if (capacity < sizes_[slot]) {
      return false;
    }
    std::memcpy(data, records_[slot].data(), sizes_[slot]);
    sizeOut = sizes_[slot];
    return true;
  }

  bool writeRecord(std::size_t slot, const std::uint8_t* data,
                   std::size_t size) noexcept override {
    if (slot >= records_.size() || size > records_[slot].size()) {
      return false;
    }
    std::memcpy(records_[slot].data(), data, size);
    sizes_[slot] = size;
    return true;
  }

  bool eraseRecord(std::size_t slot) noexcept override {
    if (slot >= records_.size()) {
      return false;
    }
    sizes_[slot] = 0;
    return true;
  }

  bool readIndex(std::uint8_t* data, std::size_t capacity,
                 std::size_t& sizeOut) const noexcept override {
    if (indexSize_ == 0 || capacity < indexSize_) {
      return false;
    }
    std::memcpy(data, index_.data(), indexSize_);
    sizeOut = indexSize_;
    return true;
  }

  bool writeIndex(const std::uint8_t* data, std::size_t size) noexcept override {
    if (size > index_.size()) {
      return false;
    }
    std::memcpy(index_.data(), data, size);
    indexSize_ = size;
    return true;
  }

  // ── Test-only helpers ──────────────────────────────────────────────

  // Flip a byte inside a slot's payload (before the CRC) to simulate a
  // corrupted record. Returns false when the slot is empty.
  bool corruptRecordPayload(std::size_t slot) noexcept {
    if (slot >= records_.size() || sizes_[slot] < 2) {
      return false;
    }
    records_[slot][0] ^= 0xFFU;  // schema byte is inside the CRC region
    return true;
  }

  // Flip a byte inside the stored index payload to simulate a corrupted
  // index. Returns false when no index is stored.
  bool corruptIndexPayload() noexcept {
    if (indexSize_ < 2) {
      return false;
    }
    index_[0] ^= 0xFFU;  // magic byte is inside the CRC region
    return true;
  }

  // Truncate the stored index to simulate a partial write.
  void truncateIndex(std::size_t size) noexcept {
    indexSize_ = size < indexSize_ ? size : indexSize_;
  }

  // Raw access so a test can patch the index bytes directly.
  std::uint8_t* indexBytes() noexcept { return index_.data(); }
  std::size_t indexSize() const noexcept { return indexSize_; }
  std::uint8_t* recordBytes(std::size_t slot) noexcept {
    return records_[slot].data();
  }
  std::size_t recordSize(std::size_t slot) const noexcept {
    return sizes_[slot];
  }

 private:
  std::vector<std::array<std::uint8_t,
                         jinggua::domain::kHistoryRecordSerializedSize>>
      records_;
  std::vector<std::size_t> sizes_;
  std::array<std::uint8_t, 64> index_{};
  std::size_t indexSize_{0};
};
