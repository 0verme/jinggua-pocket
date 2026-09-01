#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "jinggua/domain/coin.h"
#include "jinggua/hardware/display.h"

namespace jinggua::ui {

using AnimationCoinSide = domain::CoinSide;

enum class CoinAnimationState : std::uint8_t {
  Idle,
  Spinning,
  Settling,
  Finished,
};

struct CoinAnimationFrame {
  CoinAnimationFrame() noexcept {
    side = AnimationCoinSide::Back;
    widthPercent = 100;
  }
  CoinAnimationFrame(AnimationCoinSide sideValue,
                     std::uint8_t widthValue) noexcept {
    side = sideValue;
    widthPercent = widthValue;
  }

  AnimationCoinSide side;
  std::uint8_t widthPercent;
};

class CoinAnimation final {
 public:
  static constexpr std::uint32_t kSpinDurationMs = 480;
  static constexpr std::uint32_t kSettlingDurationMs = 240;
  static constexpr std::uint32_t kDurationMs =
      kSpinDurationMs + kSettlingDurationMs;
  static constexpr std::uint32_t kPhaseOffsetMs = 32;
  static constexpr std::uint32_t kSettlingFlipDurationMs = 120;

  void start(const domain::CoinResult& result, std::uint32_t nowMs) noexcept;
  bool update(std::uint32_t nowMs) noexcept;
  void reset() noexcept;

  CoinAnimationState state() const noexcept { return state_; }
  bool isActive() const noexcept {
    return state_ == CoinAnimationState::Spinning ||
           state_ == CoinAnimationState::Settling;
  }
  bool isFinished() const noexcept {
    return state_ == CoinAnimationState::Finished;
  }
  const domain::CoinSet& targetCoins() const noexcept { return targetCoins_; }
  const CoinAnimationFrame& frame(std::size_t index) const noexcept {
    return frames_[index];
  }

 private:
  static CoinAnimationFrame spinningFrame(std::uint32_t elapsedMs,
                                          std::size_t index) noexcept;
  CoinAnimationFrame settlingFrame(std::uint32_t elapsedMs,
                                   std::size_t index) const noexcept;

  domain::CoinSet targetCoins_{domain::defaultCoinSet()};
  std::array<CoinAnimationFrame, 3> frames_{};
  CoinAnimationState state_{CoinAnimationState::Idle};
  std::uint32_t startedAtMs_{0};
};

void drawCoinAnimation(hardware::Display& display,
                       const CoinAnimation& animation,
                       int centerY) noexcept;

}  // namespace jinggua::ui
