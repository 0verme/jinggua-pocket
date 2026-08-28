#include "jinggua/domain/hexagram.h"

#include <cstddef>

#include "jinggua/data/hexagrams.h"

namespace jinggua::domain {
namespace {

YinYang lineValue(const Yao& yao, LineView view) noexcept {
  return view == LineView::Original ? yao.yinYang : yao.transformedYinYang;
}

}  // namespace

std::optional<Hexagram> createHexagram(
    const std::array<Yao, 6>& lines,
    LineView view) noexcept {
  std::array<YinYang, 6> pattern{};
  std::array<YinYang, 3> lowerLines{};
  std::array<YinYang, 3> upperLines{};

  for (std::size_t index = 0; index < lines.size(); ++index) {
    if (lines[index].position != index + 1) {
      return std::nullopt;
    }
    pattern[index] = lineValue(lines[index], view);
    if (index < 3) {
      lowerLines[index] = pattern[index];
    } else {
      upperLines[index - 3] = pattern[index];
    }
  }

  const auto lower = trigramFromLines(lowerLines);
  const auto upper = trigramFromLines(upperLines);
  if (!lower.has_value() || !upper.has_value()) {
    return std::nullopt;
  }

  const auto* definition = data::findHexagram(upper->id, lower->id);
  if (definition == nullptr) {
    return std::nullopt;
  }

  Hexagram hexagram;
  hexagram.lines = lines;
  hexagram.pattern = pattern;
  hexagram.lowerTrigram = *lower;
  hexagram.upperTrigram = *upper;
  hexagram.number = definition->number;
  hexagram.name = definition->name;
  return hexagram;
}

const char* hexagramName(const Hexagram& hexagram) noexcept {
  return hexagram.name == nullptr ? "未知" : hexagram.name;
}

}  // namespace jinggua::domain
