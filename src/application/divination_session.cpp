#include "jinggua/application/divination_session.h"

#include <cstdint>

namespace jinggua::application {

DivinationSession::DivinationSession(RandomProvider& randomProvider) noexcept
    : randomProvider_(randomProvider) {}

void DivinationSession::reset() noexcept {
  lines_ = {};
  lineCount_ = 0;
  result_.reset();
}

bool DivinationSession::castLine() noexcept {
  if (isComplete()) {
    return false;
  }

  const std::array<domain::CoinSide, 3> coins{
      randomProvider_.tossCoin(),
      randomProvider_.tossCoin(),
      randomProvider_.tossCoin(),
  };
  const auto coinResult = domain::CoinResult::fromCoins(coins);
  const auto yao = domain::createYao(
      static_cast<std::uint8_t>(lineCount_ + 1), coinResult);
  if (!yao.has_value()) {
    return false;
  }

  lines_[lineCount_] = *yao;
  ++lineCount_;

  if (isComplete()) {
    result_ = domain::createDivinationResult(lines_);
    if (!result_.has_value()) {
      --lineCount_;
      lines_[lineCount_] = {};
      return false;
    }
  }
  return true;
}

const domain::Yao* DivinationSession::latestLine() const noexcept {
  if (lineCount_ == 0) {
    return nullptr;
  }
  return &lines_[lineCount_ - 1];
}

}  // namespace jinggua::application
