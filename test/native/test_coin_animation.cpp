#include "test_framework.h"

#include "jinggua/ui/coin_animation.h"

void runCoinAnimationTests(TestRunner& runner) {
  using jinggua::domain::CoinResult;
  using jinggua::domain::CoinSide;
  using jinggua::ui::CoinAnimation;
  using jinggua::ui::CoinAnimationState;

  const auto result = CoinResult::fromCoins(
      {CoinSide::Front, CoinSide::Back, CoinSide::Front});
  CoinAnimation animation;

  EXPECT_EQ(runner, animation.state(), CoinAnimationState::Idle);
  animation.start(result, 1000);
  EXPECT(runner, animation.isActive());
  EXPECT_EQ(runner, animation.targetCoins(), result.coins);

  EXPECT(runner, animation.update(1000));
  EXPECT_EQ(runner, animation.state(), CoinAnimationState::Spinning);
  EXPECT(runner, animation.frame(0).widthPercent >= 18);
  EXPECT(runner, animation.frame(0).widthPercent <= 100);

  EXPECT(runner, animation.update(1000 + CoinAnimation::kSpinDurationMs));
  EXPECT_EQ(runner, animation.state(), CoinAnimationState::Settling);
  EXPECT(runner, animation.isActive());

  const auto beforeFinish =
      1000 + CoinAnimation::kDurationMs - static_cast<std::uint32_t>(1);
  animation.update(beforeFinish);
  EXPECT_EQ(runner, animation.state(), CoinAnimationState::Settling);
  EXPECT(runner, animation.isActive());

  EXPECT(runner, animation.update(1000 + CoinAnimation::kDurationMs));
  EXPECT_EQ(runner, animation.state(), CoinAnimationState::Finished);
  EXPECT(runner, animation.isFinished());
  EXPECT(runner, !animation.isActive());
  for (std::size_t index = 0; index < result.coins.size(); ++index) {
    EXPECT_EQ(runner, animation.frame(index).side, result.coins[index]);
    EXPECT_EQ(runner, animation.frame(index).widthPercent,
              static_cast<std::uint8_t>(100));
  }

  // A second cast replaces the presentation target without generating any
  // new values: the animation still settles exactly on the supplied result.
  const auto secondResult = CoinResult::fromCoins(
      {CoinSide::Back, CoinSide::Back, CoinSide::Front});
  animation.start(secondResult, 5000);
  animation.update(5000 + CoinAnimation::kDurationMs);
  EXPECT_EQ(runner, animation.state(), CoinAnimationState::Finished);
  EXPECT_EQ(runner, animation.targetCoins(), secondResult.coins);
  for (std::size_t index = 0; index < secondResult.coins.size(); ++index) {
    EXPECT_EQ(runner, animation.frame(index).side, secondResult.coins[index]);
  }
}
