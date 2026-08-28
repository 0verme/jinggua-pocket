#include "test_framework.h"
#include "test_support.h"

#include "jinggua/application/divination_session.h"

void runOrderingTests(TestRunner& runner) {
  std::vector<jinggua::domain::CoinSide> sequence(18,
                                                   jinggua::domain::CoinSide::Front);
  SequenceRandomProvider random(std::move(sequence));
  jinggua::application::DivinationSession session(random);

  for (std::uint8_t expectedPosition = 1; expectedPosition <= 6;
       ++expectedPosition) {
    EXPECT(runner, session.castLine());
    EXPECT_EQ(runner, session.lineCount(),
              static_cast<std::size_t>(expectedPosition));
    EXPECT_EQ(runner, session.lines()[expectedPosition - 1].position,
              expectedPosition);
  }

  EXPECT_EQ(runner, session.lines()[0].position, static_cast<std::uint8_t>(1));
  EXPECT_EQ(runner, session.lines()[5].position, static_cast<std::uint8_t>(6));
  EXPECT(runner, session.isComplete());
  EXPECT_EQ(runner, random.consumed(), static_cast<std::size_t>(18));
}
