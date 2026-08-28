#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "jinggua/domain/hexagram.h"

namespace jinggua::domain {

struct DivinationResult {
  Hexagram original{};
  std::array<std::uint8_t, 6> movingPositions{};
  std::uint8_t movingCount{0};
  std::optional<Hexagram> transformed{};

  bool hasMovingLines() const noexcept { return movingCount > 0; }
};

std::optional<DivinationResult> createDivinationResult(
    const std::array<Yao, 6>& lines) noexcept;

}  // namespace jinggua::domain
