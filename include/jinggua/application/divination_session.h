#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "jinggua/application/random_provider.h"
#include "jinggua/domain/divination.h"

namespace jinggua::application {

class DivinationSession final {
 public:
  static constexpr std::size_t kLineCount = 6;

  explicit DivinationSession(RandomProvider& randomProvider) noexcept;

  void reset() noexcept;
  bool castLine() noexcept;

  std::size_t lineCount() const noexcept { return lineCount_; }
  bool isComplete() const noexcept { return lineCount_ == kLineCount; }
  const std::array<domain::Yao, kLineCount>& lines() const noexcept {
    return lines_;
  }
  const domain::Yao* latestLine() const noexcept;
  const std::optional<domain::DivinationResult>& result() const noexcept {
    return result_;
  }

 private:
  RandomProvider& randomProvider_;
  std::array<domain::Yao, kLineCount> lines_{};
  std::size_t lineCount_{0};
  std::optional<domain::DivinationResult> result_{};
};

}  // namespace jinggua::application
