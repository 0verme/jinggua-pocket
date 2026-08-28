#include "jinggua/domain/trigram.h"

#include "jinggua/data/trigrams.h"

namespace jinggua::domain {

std::optional<Trigram> trigramFromLines(
    const TrigramLineSet& lines) noexcept {
  for (const auto& trigram : data::allTrigrams()) {
    if (trigram.lines == lines) {
      return trigram;
    }
  }
  return std::nullopt;
}

const char* trigramName(TrigramId id) noexcept {
  switch (id) {
    case TrigramId::Qian:
      return "乾";
    case TrigramId::Dui:
      return "兑";
    case TrigramId::Li:
      return "离";
    case TrigramId::Zhen:
      return "震";
    case TrigramId::Xun:
      return "巽";
    case TrigramId::Kan:
      return "坎";
    case TrigramId::Gen:
      return "艮";
    case TrigramId::Kun:
      return "坤";
  }
  return "未知";
}

}  // namespace jinggua::domain
