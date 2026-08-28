#include <array>

#include "test_framework.h"
#include "test_support.h"

#include "jinggua/domain/divination.h"

void runTransformTests(TestRunner& runner) {
  using jinggua::domain::YinYang;

  const std::array<jinggua::domain::Yao, 6> lines{
      yaoAt(1, 6), yaoAt(2, 7), yaoAt(3, 8),
      yaoAt(4, 9), yaoAt(5, 8), yaoAt(6, 6),
  };
  const auto result = jinggua::domain::createDivinationResult(lines);
  EXPECT(runner, result.has_value());
  EXPECT_EQ(runner, result->movingCount, static_cast<std::uint8_t>(3));
  EXPECT_EQ(runner, result->movingPositions[0], static_cast<std::uint8_t>(1));
  EXPECT_EQ(runner, result->movingPositions[1], static_cast<std::uint8_t>(4));
  EXPECT_EQ(runner, result->movingPositions[2], static_cast<std::uint8_t>(6));
  EXPECT(runner, result->transformed.has_value());

  const auto& transformed = *result->transformed;
  EXPECT_EQ(runner, transformed.pattern[0], YinYang::Yang);
  EXPECT_EQ(runner, transformed.pattern[1], YinYang::Yang);
  EXPECT_EQ(runner, transformed.pattern[2], YinYang::Yin);
  EXPECT_EQ(runner, transformed.pattern[3], YinYang::Yin);
  EXPECT_EQ(runner, transformed.pattern[4], YinYang::Yin);
  EXPECT_EQ(runner, transformed.pattern[5], YinYang::Yang);
  EXPECT_EQ(runner, result->original.pattern[0], YinYang::Yin);
  EXPECT_EQ(runner, result->original.pattern[3], YinYang::Yang);

  const std::array<jinggua::domain::Yao, 6> stillLines{
      yaoAt(1, 7), yaoAt(2, 8), yaoAt(3, 7),
      yaoAt(4, 8), yaoAt(5, 7), yaoAt(6, 8),
  };
  const auto stillResult =
      jinggua::domain::createDivinationResult(stillLines);
  EXPECT(runner, stillResult.has_value());
  EXPECT_EQ(runner, stillResult->movingCount, static_cast<std::uint8_t>(0));
  EXPECT(runner, !stillResult->transformed.has_value());
}
