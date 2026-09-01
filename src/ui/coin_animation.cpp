#include "jinggua/ui/coin_animation.h"

#include <cstdint>

#include "jinggua/ui/layout.h"
#include "jinggua/ui/theme.h"
#include "jinggua/ui/typography.h"

namespace jinggua::ui {
namespace {

constexpr std::uint8_t kFaceWidthPercent = 100;
constexpr std::uint8_t kEdgeWidthPercent = 18;
constexpr std::uint32_t kFastSpinPeriodMs = 56;
constexpr std::uint32_t kMediumSpinPeriodMs = 76;
constexpr std::uint32_t kSlowSpinPeriodMs = 112;
constexpr std::uint32_t kFastSpinUntilMs = 160;
constexpr std::uint32_t kMediumSpinUntilMs = 320;

std::uint8_t interpolateWidth(std::uint8_t from, std::uint8_t to,
                             std::uint32_t progress,
                             std::uint32_t duration) noexcept {
  const int difference = static_cast<int>(to) - static_cast<int>(from);
  const int value = static_cast<int>(from) +
                    (difference * static_cast<int>(progress)) /
                        static_cast<int>(duration);
  return static_cast<std::uint8_t>(value);
}

std::uint32_t spinPeriodFor(std::uint32_t elapsedMs) noexcept {
  if (elapsedMs < kFastSpinUntilMs) {
    return kFastSpinPeriodMs;
  }
  if (elapsedMs < kMediumSpinUntilMs) {
    return kMediumSpinPeriodMs;
  }
  return kSlowSpinPeriodMs;
}

bool sameFrame(const CoinAnimationFrame& left,
               const CoinAnimationFrame& right) noexcept {
  return left.side == right.side && left.widthPercent == right.widthPercent;
}

domain::CoinSide oppositeSide(domain::CoinSide side) noexcept {
  return side == domain::CoinSide::Front ? domain::CoinSide::Back
                                         : domain::CoinSide::Front;
}

}  // namespace

void CoinAnimation::start(const domain::CoinResult& result,
                          std::uint32_t nowMs) noexcept {
  targetCoins_ = result.coins;
  startedAtMs_ = nowMs;
  state_ = CoinAnimationState::Spinning;
  frames_ = {};
}

bool CoinAnimation::update(std::uint32_t nowMs) noexcept {
  if (state_ == CoinAnimationState::Idle ||
      state_ == CoinAnimationState::Finished) {
    return false;
  }

  const auto previousState = state_;
  const auto previousFrames = frames_;
  const auto elapsedMs = nowMs - startedAtMs_;
  if (elapsedMs >= kDurationMs) {
    state_ = CoinAnimationState::Finished;
    for (std::size_t index = 0; index < frames_.size(); ++index) {
      frames_[index] =
          CoinAnimationFrame{targetCoins_[index], kFaceWidthPercent};
    }
  } else {
    state_ = elapsedMs < kSpinDurationMs ? CoinAnimationState::Spinning
                                         : CoinAnimationState::Settling;
    for (std::size_t index = 0; index < frames_.size(); ++index) {
      frames_[index] =
          state_ == CoinAnimationState::Spinning
              ? spinningFrame(elapsedMs, index)
              : settlingFrame(elapsedMs - kSpinDurationMs, index);
    }
  }

  if (previousState != state_) {
    return true;
  }
  for (std::size_t index = 0; index < frames_.size(); ++index) {
    if (!sameFrame(previousFrames[index], frames_[index])) {
      return true;
    }
  }
  return false;
}

void CoinAnimation::reset() noexcept {
  targetCoins_ = domain::defaultCoinSet();
  frames_ = {};
  state_ = CoinAnimationState::Idle;
  startedAtMs_ = 0;
}

CoinAnimationFrame CoinAnimation::spinningFrame(std::uint32_t elapsedMs,
                                                std::size_t index) noexcept {
  const auto period = spinPeriodFor(elapsedMs);
  const auto phase =
      elapsedMs + static_cast<std::uint32_t>(index) * kPhaseOffsetMs;
  const auto cycle = phase / period;
  const auto inCycle = phase % period;
  const auto halfPeriod = period / 2U;

  const auto width = inCycle < halfPeriod
                         ? interpolateWidth(kFaceWidthPercent,
                                            kEdgeWidthPercent, inCycle,
                                            halfPeriod)
                         : interpolateWidth(
                               kEdgeWidthPercent, kFaceWidthPercent,
                               inCycle - halfPeriod, halfPeriod);
  const bool isFront =
      ((cycle + static_cast<std::uint32_t>(index)) % 2U) == 0U;
  return CoinAnimationFrame{isFront ? domain::CoinSide::Front
                                    : domain::CoinSide::Back,
                            width};
}

CoinAnimationFrame CoinAnimation::settlingFrame(std::uint32_t elapsedMs,
                                                std::size_t index) const
    noexcept {
  const auto delay = static_cast<std::uint32_t>(index) * kPhaseOffsetMs;
  if (elapsedMs < delay) {
    return spinningFrame(kSpinDurationMs + elapsedMs, index);
  }

  const auto flipElapsed = elapsedMs - delay;
  const auto halfFlip = kSettlingFlipDurationMs / 2U;
  if (flipElapsed < halfFlip) {
    return CoinAnimationFrame{
        oppositeSide(targetCoins_[index]),
        interpolateWidth(kFaceWidthPercent, kEdgeWidthPercent, flipElapsed,
                         halfFlip)};
  }
  if (flipElapsed < kSettlingFlipDurationMs) {
    return CoinAnimationFrame{
        targetCoins_[index],
        interpolateWidth(kEdgeWidthPercent, kFaceWidthPercent,
                         flipElapsed - halfFlip, halfFlip)};
  }
  return CoinAnimationFrame{targetCoins_[index], kFaceWidthPercent};
}

void drawCoinAnimation(hardware::Display& display,
                       const CoinAnimation& animation,
                       int centerY) noexcept {
  for (std::size_t index = 0; index < 3; ++index) {
    const auto& frame = animation.frame(index);
    const int coinCenterX = layout::coinCenterX(display.width(), index);
    const int radiusX = layout::kCoinAnimationMinRadiusX +
                        (layout::kCoinAnimationRadiusXRange *
                         static_cast<int>(frame.widthPercent)) /
                            100;

    display.fillEllipse(coinCenterX, centerY, radiusX,
                        layout::kCoinAnimationRadiusY, theme::kAccent);
    display.drawEllipse(coinCenterX, centerY, radiusX,
                        layout::kCoinAnimationRadiusY, theme::kText);
    if (radiusX >= layout::kCoinAnimationVisibleRadiusX) {
      const char* symbol =
          frame.side == domain::CoinSide::Front ? "●" : "○";
      display.drawText(symbol,
                       coinCenterX - layout::kCoinAnimationSymbolOffset,
                       centerY - layout::kCoinAnimationSymbolYOffset,
                       theme::kBackground, typography::kSecondary);
    }
  }
}

}  // namespace jinggua::ui
