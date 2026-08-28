#include "jinggua/domain/coin.h"

namespace jinggua::domain {

CoinResult CoinResult::fromCoins(const CoinSet& coinsValue) noexcept {
  const auto total = static_cast<std::uint8_t>(coinValue(coinsValue[0]) +
                                                coinValue(coinsValue[1]) +
                                                coinValue(coinsValue[2]));
  return CoinResult{coinsValue, total};
}

}  // namespace jinggua::domain
