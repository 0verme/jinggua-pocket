#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "jinggua/domain/trigram.h"

namespace jinggua::domain {

enum class LineView : std::uint8_t {
  Original,
  Transformed,
};

struct Hexagram {
  // Both arrays use index 0 = 初爻 and index 5 = 上爻.
  std::array<Yao, 6> lines{};
  std::array<YinYang, 6> pattern{};
  Trigram lowerTrigram{};
  Trigram upperTrigram{};
  std::uint8_t number{0};
  const char* name{""};
};

std::optional<Hexagram> createHexagram(
    const std::array<Yao, 6>& lines,
    LineView view = LineView::Original) noexcept;
const char* hexagramName(const Hexagram& hexagram) noexcept;

}  // namespace jinggua::domain
