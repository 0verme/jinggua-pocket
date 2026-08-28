#pragma once

#include <array>
#include <cstdint>

namespace jinggua::domain {

enum class CoinSide : std::uint8_t {
  Back = 2,
  Front = 3,
};

constexpr std::uint8_t coinValue(CoinSide side) noexcept {
  return static_cast<std::uint8_t>(side);
}

using CoinSet = std::array<CoinSide, 3>;

constexpr CoinSet defaultCoinSet() noexcept {
  return CoinSet{CoinSide::Back, CoinSide::Back, CoinSide::Back};
}

struct CoinResult {
  CoinSet coins{defaultCoinSet()};
  std::uint8_t total{0};

  static CoinResult fromCoins(const CoinSet& coins) noexcept;
};

}  // namespace jinggua::domain
