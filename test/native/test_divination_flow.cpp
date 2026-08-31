#include <array>
#include <cstring>
#include <utility>
#include <vector>

#include "test_framework.h"
#include "test_support.h"

#include "jinggua/application/divination_session.h"
#include "jinggua/domain/divination.h"
#include "jinggua/domain/yao.h"

namespace {

void appendCoinResult(std::vector<jinggua::domain::CoinSide>& sequence,
                      std::uint8_t total) {
  const auto result = coinsForTotal(total);
  sequence.insert(sequence.end(), result.coins.begin(), result.coins.end());
}

SequenceRandomProvider randomForTotals(
    const std::array<std::uint8_t, 6>& totals) {
  std::vector<jinggua::domain::CoinSide> sequence;
  sequence.reserve(totals.size() * 3U);
  for (const auto total : totals) {
    appendCoinResult(sequence, total);
  }
  return SequenceRandomProvider(std::move(sequence));
}

}  // namespace

void runDivinationFlowTests(TestRunner& runner) {
  using jinggua::domain::YaoType;

  // From bottom to top: 6, 7, 8, 9, 8, 6 resolves 雷水解 (#40) to
  // 山泽损 (#41), with the first, fourth, and sixth lines moving.
  auto random = randomForTotals({6, 7, 8, 9, 8, 6});
  jinggua::application::DivinationSession session(random);
  for (std::size_t index = 0; index < 6; ++index) {
    EXPECT(runner, session.castLine());
    EXPECT_EQ(runner, session.lines()[index].position,
              static_cast<std::uint8_t>(index + 1));
  }

  EXPECT(runner, session.isComplete());
  EXPECT_EQ(runner, random.consumed(), static_cast<std::size_t>(18));
  EXPECT_EQ(runner, session.lines()[0].type, YaoType::OldYin);
  EXPECT_EQ(runner, session.lines()[3].type, YaoType::OldYang);
  EXPECT(runner, session.result().has_value());
  if (session.result().has_value()) {
    const auto& result = *session.result();
    EXPECT_EQ(runner, result.original.number, static_cast<std::uint8_t>(40));
    EXPECT(runner, std::strcmp(result.original.name, "雷水解") == 0);
    EXPECT_EQ(runner, result.movingCount, static_cast<std::uint8_t>(3));
    EXPECT_EQ(runner, result.movingPositions[0], static_cast<std::uint8_t>(1));
    EXPECT_EQ(runner, result.movingPositions[1], static_cast<std::uint8_t>(4));
    EXPECT_EQ(runner, result.movingPositions[2], static_cast<std::uint8_t>(6));
    EXPECT(runner, result.transformed.has_value());
    if (result.transformed.has_value()) {
      EXPECT_EQ(runner, result.transformed->number,
                static_cast<std::uint8_t>(41));
      EXPECT(runner, std::strcmp(result.transformed->name, "山泽损") == 0);
    }
  }

  // Six static lines produce a result without a transformed hexagram.
  auto stillRandom = randomForTotals({7, 8, 7, 8, 7, 8});
  jinggua::application::DivinationSession stillSession(stillRandom);
  for (std::size_t index = 0; index < 6; ++index) {
    EXPECT(runner, stillSession.castLine());
  }
  EXPECT(runner, stillSession.result().has_value());
  if (stillSession.result().has_value()) {
    EXPECT_EQ(runner, stillSession.result()->original.number,
              static_cast<std::uint8_t>(63));
    EXPECT(runner, !stillSession.result()->hasMovingLines());
    EXPECT(runner, !stillSession.result()->transformed.has_value());
  }

  // Six moving lines exercise the opposite extreme: 坤为地 (#2) becomes
  // 乾为天 (#1), and every position is reported as moving.
  auto allMovingRandom = randomForTotals({6, 6, 6, 6, 6, 6});
  jinggua::application::DivinationSession allMovingSession(allMovingRandom);
  for (std::size_t index = 0; index < 6; ++index) {
    EXPECT(runner, allMovingSession.castLine());
  }
  EXPECT(runner, allMovingSession.result().has_value());
  if (allMovingSession.result().has_value()) {
    const auto& result = *allMovingSession.result();
    EXPECT_EQ(runner, result.original.number, static_cast<std::uint8_t>(2));
    EXPECT_EQ(runner, result.movingCount, static_cast<std::uint8_t>(6));
    for (std::size_t index = 0; index < 6; ++index) {
      EXPECT_EQ(runner, result.movingPositions[index],
                static_cast<std::uint8_t>(index + 1));
    }
    EXPECT(runner, result.transformed.has_value());
    if (result.transformed.has_value()) {
      EXPECT_EQ(runner, result.transformed->number,
                static_cast<std::uint8_t>(1));
    }
  }
}
