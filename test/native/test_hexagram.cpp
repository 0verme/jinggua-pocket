#include <array>
#include <cstring>

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
  for (std::size_t index = 0; index < definitions.size(); ++index) {
    const auto& definition = definitions[index];
    EXPECT_EQ(runner, definition.number,
              static_cast<std::uint8_t>(index + 1));
    EXPECT(runner, jinggua::data::findHexagram(definition.upper,
                                               definition.lower) != nullptr);
  }
}
