#include "test_framework.h"

#include "jinggua/domain/coin.h"

void runCoinTests(TestRunner& runner) {
  using jinggua::domain::CoinResult;
  using jinggua::domain::CoinSide;

  const auto oldYang = CoinResult::fromCoins(
      {CoinSide::Front, CoinSide::Front, CoinSide::Front});
  EXPECT_EQ(runner, oldYang.total, static_cast<std::uint8_t>(9));

  const auto oldYin = CoinResult::fromCoins(
      {CoinSide::Back, CoinSide::Back, CoinSide::Back});
  EXPECT_EQ(runner, oldYin.total, static_cast<std::uint8_t>(6));

  const auto youngYang = CoinResult::fromCoins(
      {CoinSide::Front, CoinSide::Back, CoinSide::Back});
  EXPECT_EQ(runner, youngYang.total, static_cast<std::uint8_t>(7));

  const auto youngYin = CoinResult::fromCoins(
      {CoinSide::Front, CoinSide::Front, CoinSide::Back});
  EXPECT_EQ(runner, youngYin.total, static_cast<std::uint8_t>(8));
  EXPECT_EQ(runner, youngYin.coins[0], CoinSide::Front);
  EXPECT_EQ(runner, youngYin.coins[2], CoinSide::Back);
}
