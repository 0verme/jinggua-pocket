#include "jinggua/domain/divination.h"

#include <cstddef>

namespace jinggua::domain {

std::optional<DivinationResult> createDivinationResult(
    const std::array<Yao, 6>& lines) noexcept {
  const auto original = createHexagram(lines, LineView::Original);
  if (!original.has_value()) {
    return std::nullopt;
  }

  DivinationResult result;
  result.original = *original;
  for (std::size_t index = 0; index < lines.size(); ++index) {
    if (lines[index].moving) {
      result.movingPositions[result.movingCount] = lines[index].position;
      ++result.movingCount;
    }
  }

  if (result.hasMovingLines()) {
    result.transformed = createHexagram(lines, LineView::Transformed);
    if (!result.transformed.has_value()) {
      return std::nullopt;
    }
  }

  return result;
}

}  // namespace jinggua::domain
