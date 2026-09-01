#pragma once

#include <cstddef>

#include "jinggua/ui/typography.h"

namespace jinggua::ui::layout {

// The product canvas is portrait 135x240.  The helpers keep the renderer
// readable on the target panel while still deriving edge positions from the
// display dimensions supplied by the hardware adapter.
constexpr int kScreenWidth = 135;
constexpr int kScreenHeight = 240;
constexpr int kMargin = 10;
constexpr int kSafeTop = 6;
constexpr int kSafeBottom = 8;

constexpr int kHeaderY = kSafeTop;
constexpr int kContentY = 44;
constexpr int kFooterLineHeight = typography::kSecondaryLineHeight;
constexpr int kFooterGap = 4;
constexpr int kFooterY = kScreenHeight - kSafeBottom - kFooterLineHeight;
constexpr int kFooterSecondaryY =
    kFooterY - kFooterLineHeight - kFooterGap;

constexpr int kHomeTitleY = 18;
constexpr int kHomePrimaryY = 94;
constexpr int kHomeSecondaryY = 130;

constexpr int kPreparePrimaryY = 60;
constexpr int kPrepareSecondaryY = 106;

constexpr int kCastingCoinsY = 100;
constexpr int kCastingActionY = 164;

constexpr int kLineAnimationY = 88;
constexpr int kLineAnimationStatusY = 130;
constexpr int kLineResultCoinsY = 68;
constexpr int kLineResultValueY = 110;
constexpr int kLineResultMovingY = 150;

constexpr int kResultNameY = 42;
constexpr int kResultNumberY = 78;
constexpr int kResultMovingY = 96;
constexpr int kHexagramTop = 116;
constexpr int kHexagramLineSpacing = 15;
constexpr int kHexagramLineWidth = 88;
constexpr int kHexagramYinGap = 10;
constexpr int kHexagramMarkerGap = 7;
constexpr int kHexagramMarkerWidth = 14;
constexpr int kHexagramMarkerYOffset = 7;

constexpr int kResetQuestionY = 70;

constexpr int kWifiStatusY = 62;
constexpr int kWifiSsidY = 104;
constexpr std::size_t kWifiSsidMaxCharacters = 8;

constexpr int kHistoryOrderY = 42;
constexpr int kHistoryEmptyY = 62;
constexpr int kHistoryEmptyHintY = 108;
constexpr int kHistoryOriginalLabelY = 62;
constexpr int kHistoryOriginalNameY = 82;
constexpr int kHistoryMovingY = 118;
constexpr int kHistoryChangedLabelY = 140;
constexpr int kHistoryChangedNameY = 158;

constexpr int kCoinSpacing = 36;
constexpr int kCoinPromptRadiusX = 14;
constexpr int kCoinPromptRadiusY = 20;
constexpr int kCoinResultRadiusX = 13;
constexpr int kCoinResultRadiusY = 18;
constexpr int kCoinAnimationRadiusY = 22;
constexpr int kCoinAnimationMinRadiusX = 2;
constexpr int kCoinAnimationRadiusXRange = 16;
constexpr int kCoinAnimationVisibleRadiusX = 6;
constexpr int kCoinSymbolHalfWidth = 12;
constexpr int kCoinPromptSymbolOffset = 6;
constexpr int kCoinResultSymbolYOffset = 8;
constexpr int kCoinAnimationSymbolOffset = 6;
constexpr int kCoinAnimationSymbolYOffset = 6;

constexpr int kHexagramLastY = kHexagramTop + 5 * kHexagramLineSpacing;
constexpr int kHexagramMarkerLastY =
    kHexagramLastY - kHexagramMarkerYOffset +
    typography::kSecondaryLineHeight;

constexpr int horizontalMargin(int width) noexcept {
  if (width <= 2 * kMargin) {
    return width > 0 ? width / 2 : 0;
  }
  return width >= kScreenWidth ? kMargin : 8;
}

constexpr int footerY(int height) noexcept {
  return height > kSafeBottom + kFooterLineHeight
             ? height - kSafeBottom - kFooterLineHeight
             : 0;
}

constexpr int footerSecondaryY(int height) noexcept {
  const int primary = footerY(height);
  return primary >= kFooterLineHeight + kFooterGap
             ? primary - kFooterLineHeight - kFooterGap
             : 0;
}

constexpr int coinSpacing(int width) noexcept {
  return width >= kScreenWidth ? kCoinSpacing : 28;
}

constexpr int coinCenterX(int width, std::size_t index) noexcept {
  return width / 2 + (static_cast<int>(index) - 1) * coinSpacing(width);
}

constexpr int hexagramLineWidth(int width) noexcept {
  const int available = width - 2 * kMargin - kHexagramMarkerGap -
                        kHexagramMarkerWidth;
  if (available <= 0) {
    return 1;
  }
  return available < kHexagramLineWidth ? available : kHexagramLineWidth;
}

constexpr int hexagramLeft(int width) noexcept {
  const int visualWidth = hexagramLineWidth(width) + kHexagramMarkerGap +
                          kHexagramMarkerWidth;
  return (width - visualWidth) / 2;
}

constexpr int hexagramLineSpacing(int height) noexcept {
  if (height >= kScreenHeight) {
    return kHexagramLineSpacing;
  }
  const int available = footerY(height) - kHexagramTop - kSafeBottom;
  return available >= 5 ? available / 5 : 1;
}

constexpr bool isPocketLayoutSafe() noexcept {
  return kHeaderY >= kSafeTop &&
         kFooterY + kFooterLineHeight <= kScreenHeight - kSafeBottom &&
         kContentY > kHeaderY + typography::kSectionLineHeight &&
         kHexagramLastY < kFooterSecondaryY &&
         kHexagramMarkerLastY < kFooterY &&
         kHomePrimaryY + typography::kPrimaryLineHeight < kFooterY &&
         kCastingActionY + typography::kPrimaryLineHeight < kFooterY;
}

static_assert(kScreenWidth == 135 && kScreenHeight == 240);
static_assert(isPocketLayoutSafe());

}  // namespace jinggua::ui::layout
