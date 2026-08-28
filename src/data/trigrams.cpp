#include "jinggua/data/trigrams.h"

namespace jinggua::data {

const std::array<domain::Trigram, 8>& allTrigrams() noexcept {
  static const std::array<domain::Trigram, 8> values{
      domain::Trigram{domain::TrigramId::Qian,
                      {domain::YinYang::Yang, domain::YinYang::Yang,
                       domain::YinYang::Yang},
                      "乾",
                      7},
      domain::Trigram{domain::TrigramId::Dui,
                      {domain::YinYang::Yang, domain::YinYang::Yang,
                       domain::YinYang::Yin},
                      "兑",
                      3},
      domain::Trigram{domain::TrigramId::Li,
                      {domain::YinYang::Yang, domain::YinYang::Yin,
                       domain::YinYang::Yang},
                      "离",
                      5},
      domain::Trigram{domain::TrigramId::Zhen,
                      {domain::YinYang::Yang, domain::YinYang::Yin,
                       domain::YinYang::Yin},
                      "震",
                      1},
      domain::Trigram{domain::TrigramId::Xun,
                      {domain::YinYang::Yin, domain::YinYang::Yang,
                       domain::YinYang::Yang},
                      "巽",
                      6},
      domain::Trigram{domain::TrigramId::Kan,
                      {domain::YinYang::Yin, domain::YinYang::Yang,
                       domain::YinYang::Yin},
                      "坎",
                      2},
      domain::Trigram{domain::TrigramId::Gen,
                      {domain::YinYang::Yin, domain::YinYang::Yin,
                       domain::YinYang::Yang},
                      "艮",
                      4},
      domain::Trigram{domain::TrigramId::Kun,
                      {domain::YinYang::Yin, domain::YinYang::Yin,
                       domain::YinYang::Yin},
                      "坤",
                      0},
  };
  return values;
}

const domain::Trigram* findTrigram(domain::TrigramId id) noexcept {
  for (const auto& trigram : allTrigrams()) {
    if (trigram.id == id) {
      return &trigram;
    }
  }
  return nullptr;
}

}  // namespace jinggua::data
