#pragma once

#include <cstdint>

#include "jinggua/domain/coin.h"
#include "jinggua/ui/theme.h"

namespace jinggua::ui::coin_visual {

// A 26 px coin fits the 135 px display with the existing 36 px center spacing.
// Animation compresses only the horizontal radius; the settled face is a true
// circle (13 px in both axes).
constexpr int kRadius = 13;
constexpr int kMinAnimationRadiusX = 2;
constexpr int kFaceDetailMinRadiusX = 7;
constexpr int kHoleSize = 5;
constexpr std::uint8_t kFullWidthPercent = 100;

constexpr hardware::Color kCopperBody = 0xBB26;       // warm copper orange
constexpr hardware::Color kCopperEdge = 0x5983;       // deep copper-brown
constexpr hardware::Color kCopperRim = 0xCC89;        // muted gold
constexpr hardware::Color kCopperStud = 0xD4EA;       // restrained highlight
constexpr hardware::Color kCopperEngraving = 0x69A3;  // dark stamped detail

constexpr int horizontalRadius(std::uint8_t widthPercent) noexcept {
  const int clampedPercent =
      widthPercent > kFullWidthPercent ? kFullWidthPercent : widthPercent;
  const int radius = (kRadius * clampedPercent) / kFullWidthPercent;
  return radius < kMinAnimationRadiusX ? kMinAnimationRadiusX : radius;
}

template <typename Surface>
void drawCoin(Surface& surface, domain::CoinSide side, int centerX,
              int centerY,
              std::uint8_t widthPercent = kFullWidthPercent) noexcept {
  const int radiusX = horizontalRadius(widthPercent);
  surface.fillEllipse(centerX, centerY, radiusX, kRadius, kCopperBody);
  surface.drawEllipse(centerX, centerY, radiusX, kRadius, kCopperEdge);

  // During the edge-on part of the flip, suppress face details instead of
  // stretching or clipping the hole and decorative marks.
  if (radiusX < kFaceDetailMinRadiusX) {
    return;
  }

  surface.drawEllipse(centerX, centerY, radiusX - 2, kRadius - 2, kCopperRim);
  surface.drawLine(centerX - 4, centerY - 9, centerX + 1, centerY - 9,
                   kCopperStud);

  if (side == domain::CoinSide::Front) {
    // Four small, restrained studs distinguish the Front without tiny text.
    surface.fillEllipse(centerX, centerY - 7, 1, 1, kCopperStud);
    surface.fillEllipse(centerX + 7, centerY, 1, 1, kCopperStud);
    surface.fillEllipse(centerX, centerY + 7, 1, 1, kCopperStud);
    surface.fillEllipse(centerX - 7, centerY, 1, 1, kCopperStud);
  } else {
    // Paired vertical reverse-side grooves use the same coin vocabulary.
    surface.drawLine(centerX - 5, centerY - 2, centerX - 5, centerY + 2,
                     kCopperEngraving);
    surface.drawLine(centerX + 5, centerY - 2, centerX + 5, centerY + 2,
                     kCopperEngraving);
  }

  const int holeOffset = kHoleSize / 2;
  surface.fillRect(centerX - holeOffset, centerY - holeOffset, kHoleSize,
                   kHoleSize, theme::kBackground);
}

}  // namespace jinggua::ui::coin_visual
