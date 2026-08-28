#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "jinggua/application/random_provider.h"
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
