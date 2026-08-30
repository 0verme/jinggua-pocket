#include <array>
#include <cstring>
#include <set>
#include <string>

#include "test_framework.h"
#include "test_support.h"

#include "jinggua/data/hexagrams.h"
#include "jinggua/data/trigrams.h"
#include "jinggua/domain/hexagram.h"

void runHexagramTests(TestRunner& runner) {
  using jinggua::domain::TrigramId;
  using jinggua::domain::YinYang;

  // Lower Kan (阴阳阴) + upper Zhen (阳阴阴) = 雷水解, #40.
  const std::array<jinggua::domain::Yao, 6> lines{
      yaoAt(1, 8), yaoAt(2, 7), yaoAt(3, 8),
      yaoAt(4, 7), yaoAt(5, 8), yaoAt(6, 8),
  };
  const auto hexagram = jinggua::domain::createHexagram(lines);
  EXPECT(runner, hexagram.has_value());
  EXPECT_EQ(runner, hexagram->lowerTrigram.id, TrigramId::Kan);
  EXPECT_EQ(runner, hexagram->upperTrigram.id, TrigramId::Zhen);
  EXPECT_EQ(runner, hexagram->number, static_cast<std::uint8_t>(40));
  EXPECT(runner, std::strcmp(hexagram->name, "雷水解") == 0);
  EXPECT_EQ(runner, hexagram->pattern[0], YinYang::Yin);
  EXPECT_EQ(runner, hexagram->pattern[5], YinYang::Yin);

  const auto& trigrams = jinggua::data::allTrigrams();
  EXPECT_EQ(runner, trigrams.size(), static_cast<std::size_t>(8));
  for (const auto& trigram : trigrams) {
    const auto parsed = jinggua::domain::trigramFromLines(trigram.lines);
    EXPECT(runner, parsed.has_value());
    EXPECT_EQ(runner, parsed->id, trigram.id);
  }

  const auto& definitions = jinggua::data::allHexagrams();
  EXPECT_EQ(runner, definitions.size(), static_cast<std::size_t>(64));
  std::array<bool, 65> seenNumbers{};
  std::set<std::string> names;
  for (const auto& definition : definitions) {
    EXPECT(runner, definition.number >= 1 && definition.number <= 64);
    EXPECT(runner, !seenNumbers[definition.number]);
    seenNumbers[definition.number] = true;
    EXPECT(runner, definition.name != nullptr && definition.name[0] != '\0');
    EXPECT(runner, names.emplace(definition.name).second);
    EXPECT(runner, jinggua::data::findHexagram(definition.upper,
                                               definition.lower) != nullptr);
  }
  for (std::size_t number = 1; number <= 64; ++number) {
    EXPECT(runner, seenNumbers[number]);
  }

  // Every ordered lower/upper trigram pair must resolve to exactly one
  // canonical hexagram (8 x 8 = 64), not merely the listed definitions.
  const auto& allTrigrams = jinggua::data::allTrigrams();
  for (const auto& upper : allTrigrams) {
    for (const auto& lower : allTrigrams) {
      std::array<jinggua::domain::Yao, 6> pairLines{};
      for (std::size_t index = 0; index < 3; ++index) {
        pairLines[index] = yaoAt(
            static_cast<std::uint8_t>(index + 1),
            lower.lines[index] == YinYang::Yang ? 7 : 8);
        pairLines[index + 3] = yaoAt(
            static_cast<std::uint8_t>(index + 4),
            upper.lines[index] == YinYang::Yang ? 7 : 8);
      }
      const auto resolved = jinggua::domain::createHexagram(pairLines);
      EXPECT(runner, resolved.has_value());
      if (resolved.has_value()) {
        EXPECT_EQ(runner, resolved->lowerTrigram.id, lower.id);
        EXPECT_EQ(runner, resolved->upperTrigram.id, upper.id);
        EXPECT(runner, resolved->number >= 1 && resolved->number <= 64);
        EXPECT(runner, resolved->name != nullptr && resolved->name[0] != '\0');
      }
    }
  }
}
