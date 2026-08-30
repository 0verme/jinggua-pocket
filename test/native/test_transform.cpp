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
  // Exhaust all 4^6 line-type combinations: every transformed result must
  // remain one of the canonical 64 hexagrams, and only moving lines produce
  // a transformed semantic result.
  for (std::uint16_t encoded = 0; encoded < 4096; ++encoded) {
    std::uint16_t value = encoded;
    std::array<jinggua::domain::Yao, 6> exhaustiveLines{};
    bool hasMovingLines = false;
    for (std::size_t index = 0; index < exhaustiveLines.size(); ++index) {
      const auto total = static_cast<std::uint8_t>(6 + (value % 4));
      value = static_cast<std::uint16_t>(value / 4);
      exhaustiveLines[index] = yaoAt(
          static_cast<std::uint8_t>(index + 1), total);
      hasMovingLines = hasMovingLines || exhaustiveLines[index].moving;
    }
    const auto exhaustiveResult =
        jinggua::domain::createDivinationResult(exhaustiveLines);
    EXPECT(runner, exhaustiveResult.has_value());
    if (exhaustiveResult.has_value()) {
      EXPECT(runner, exhaustiveResult->original.number >= 1 &&
                         exhaustiveResult->original.number <= 64);
      EXPECT_EQ(runner, exhaustiveResult->hasMovingLines(), hasMovingLines);
      EXPECT_EQ(runner, exhaustiveResult->transformed.has_value(),
                hasMovingLines);
      if (hasMovingLines) {
        EXPECT(runner, exhaustiveResult->transformed->number >= 1 &&
                           exhaustiveResult->transformed->number <= 64);
      }
    }
  }
}
