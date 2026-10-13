#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include "test_framework.h"
#include "test_support.h"

#include "jinggua/application/divination_session.h"
#include "jinggua/application/offline_hexagram_code.h"
#include "jinggua/domain/divination.h"
#include "jinggua/domain/history_record.h"
#include "jinggua/domain/offline_hexagram_code.h"

namespace {

using LineValues = std::array<std::uint8_t, 6>;
using jinggua::domain::OfflineHexagramCode;

std::vector<jinggua::domain::CoinSide> coinSequenceFor(
    const LineValues& lineValues) {
  std::vector<jinggua::domain::CoinSide> sequence;
  sequence.reserve(lineValues.size() * 3U);
  for (const auto total : lineValues) {
    const auto coins = coinsForTotal(total);
    sequence.insert(sequence.end(), coins.coins.begin(), coins.coins.end());
  }
  return sequence;
}

void expectMovingLineCount(TestRunner& runner, const LineValues& lineValues,
                           std::uint8_t expectedMovingCount) {
  std::array<jinggua::domain::Yao, 6> lines{};
  for (std::size_t index = 0; index < lines.size(); ++index) {
    const auto yao = jinggua::domain::createYao(
        static_cast<std::uint8_t>(index + 1U), coinsForTotal(lineValues[index]));
    EXPECT(runner, yao.has_value());
    if (yao.has_value()) {
      lines[index] = *yao;
    }
  }
  const auto result = jinggua::domain::createDivinationResult(lines);
  EXPECT(runner, result.has_value());
  if (result.has_value()) {
    EXPECT_EQ(runner, result->movingCount, expectedMovingCount);
  }
}

}  // namespace

void runOfflineHexagramCodeTests(TestRunner& runner) {
  using jinggua::application::DivinationSession;
  using jinggua::application::offlineHexagramCodeForSession;
  using jinggua::domain::encodeOfflineHexagramCode;
  using jinggua::domain::formatOfflineHexagramCode;

  // Exhaustively enumerate every Base-4 combination. Its digits represent the
  // six line values from bottom to top, and each encoded number must be unique.
  std::array<bool, 4096> seen{};
  for (std::uint32_t expected = 0; expected <= 4095; ++expected) {
    auto digits = expected;
    LineValues lineValues{};
    for (auto& lineValue : lineValues) {
      lineValue = static_cast<std::uint8_t>(6U + (digits & 0x03U));
      digits >>= 2U;
    }

    const auto code = encodeOfflineHexagramCode(lineValues);
    EXPECT(runner, code.has_value());
    if (!code.has_value()) {
      continue;
    }
    EXPECT(runner, *code <= jinggua::domain::kOfflineHexagramCodeMaximum);
    EXPECT_EQ(runner, *code, static_cast<OfflineHexagramCode>(expected));
    EXPECT(runner, !seen[*code]);
    seen[*code] = true;
  }
  for (const bool found : seen) {
    EXPECT(runner, found);
  }

  const LineValues sample{6, 7, 8, 9, 8, 6};
  const auto sampleCode = encodeOfflineHexagramCode(sample);
  EXPECT(runner, sampleCode.has_value());
  if (sampleCode.has_value()) {
    EXPECT_EQ(runner, *sampleCode, static_cast<OfflineHexagramCode>(740));
    char formatted[5]{};
    EXPECT(runner, formatOfflineHexagramCode(*sampleCode, formatted,
                                             sizeof(formatted)));
    EXPECT(runner, std::strcmp(formatted, "0740") == 0);
  }

  const auto five = encodeOfflineHexagramCode({7, 7, 6, 6, 6, 6});
  EXPECT(runner, five.has_value());
  if (five.has_value()) {
    EXPECT_EQ(runner, *five, static_cast<OfflineHexagramCode>(5));
    char formatted[5]{};
    EXPECT(runner, formatOfflineHexagramCode(*five, formatted,
                                             sizeof(formatted)));
    EXPECT(runner, std::strcmp(formatted, "0005") == 0);
  }

  const auto allOldYin = encodeOfflineHexagramCode({6, 6, 6, 6, 6, 6});
  const auto allOldYang = encodeOfflineHexagramCode({9, 9, 9, 9, 9, 9});
  EXPECT(runner, allOldYin.has_value());
  EXPECT(runner, allOldYang.has_value());
  if (allOldYin.has_value() && allOldYang.has_value()) {
    EXPECT_EQ(runner, *allOldYin, static_cast<OfflineHexagramCode>(0));
    EXPECT_EQ(runner, *allOldYang, static_cast<OfflineHexagramCode>(4095));
    char formatted[5]{};
    EXPECT(runner,
           formatOfflineHexagramCode(*allOldYin, formatted, sizeof(formatted)));
    EXPECT(runner, std::strcmp(formatted, "0000") == 0);
    EXPECT(runner, formatOfflineHexagramCode(*allOldYang, formatted,
                                             sizeof(formatted)));
    EXPECT(runner, std::strcmp(formatted, "4095") == 0);
  }

  LineValues invalidLow{6, 7, 8, 9, 8, 6};
  invalidLow[0] = 5;
  LineValues invalidHigh{6, 7, 8, 9, 8, 6};
  invalidHigh[5] = 10;
  EXPECT(runner, !encodeOfflineHexagramCode(invalidLow).has_value());
  EXPECT(runner, !encodeOfflineHexagramCode(invalidHigh).has_value());
  char shortBuffer[4]{};
  EXPECT(runner, !formatOfflineHexagramCode(740, shortBuffer,
                                            sizeof(shortBuffer)));
  EXPECT(runner, !formatOfflineHexagramCode(740, nullptr, 5));
  char formatted[5]{};
  EXPECT(runner, !formatOfflineHexagramCode(4096, formatted,
                                            sizeof(formatted)));

  // The code retains every line value regardless of whether it is moving.
  const LineValues noMoving{7, 8, 7, 8, 7, 8};
  const LineValues oneMoving{6, 7, 7, 7, 7, 7};
  expectMovingLineCount(runner, noMoving, 0);
  expectMovingLineCount(runner, oneMoving, 1);
  expectMovingLineCount(runner, sample, 3);
  expectMovingLineCount(runner, {6, 9, 6, 9, 6, 9}, 6);
  EXPECT_EQ(runner, encodeOfflineHexagramCode(noMoving),
            std::optional<OfflineHexagramCode>(
                static_cast<OfflineHexagramCode>(2457)));
  EXPECT_EQ(runner, encodeOfflineHexagramCode(oneMoving),
            std::optional<OfflineHexagramCode>(
                static_cast<OfflineHexagramCode>(1364)));
  EXPECT_EQ(runner, encodeOfflineHexagramCode(sample),
            std::optional<OfflineHexagramCode>(
                static_cast<OfflineHexagramCode>(740)));

  // A session has no available/displayable code until all six lines have
  // produced a complete DivinationResult. Re-reading the result is stable and
  // doesn't mutate the session or its existing hexagram data.
  SequenceRandomProvider random(coinSequenceFor(sample));
  DivinationSession session(random);
  EXPECT(runner, !offlineHexagramCodeForSession(session).has_value());
  for (std::size_t index = 0; index < sample.size(); ++index) {
    EXPECT(runner, session.castLine());
    if (index + 1U < sample.size()) {
      EXPECT(runner, !offlineHexagramCodeForSession(session).has_value());
    }
  }
  EXPECT(runner, session.isComplete());
  EXPECT(runner, session.result().has_value());
  const auto sessionCode = offlineHexagramCodeForSession(session);
  EXPECT(runner, sessionCode.has_value());
  if (sessionCode.has_value()) {
    EXPECT_EQ(runner, *sessionCode, static_cast<OfflineHexagramCode>(740));
    const auto originalNumber = session.result()->original.number;
    const auto repeatedCode = offlineHexagramCodeForSession(session);
    EXPECT_EQ(runner, repeatedCode, sessionCode);
    EXPECT_EQ(runner, session.lines()[0].coins.total, sample[0]);
    EXPECT_EQ(runner, session.lines()[5].coins.total, sample[5]);
    EXPECT_EQ(runner, session.result()->original.number, originalNumber);
    EXPECT(runner, session.result()->transformed.has_value());
    if (session.result()->transformed.has_value()) {
      EXPECT_EQ(runner, session.result()->transformed->number,
                static_cast<std::uint8_t>(41));
    }
  }

  // History already stores the six source totals; deriving from them produces
  // the same code without adding a persisted code/history identifier field.
  if (session.result().has_value()) {
    const auto record = jinggua::domain::makeHistoryRecord(*session.result());
    EXPECT(runner, record.has_value());
    if (record.has_value()) {
      const auto historyCode = encodeOfflineHexagramCode(record->lineTotals);
      EXPECT_EQ(runner, historyCode, sessionCode);
    }
  }
}
