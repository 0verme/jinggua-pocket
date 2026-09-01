#include "jinggua/data/hexagrams.h"

namespace jinggua::data {

const std::array<HexagramDefinition, 64>& allHexagrams() noexcept {
  static const std::array<HexagramDefinition, 64> values{{
      {1, "乾为天", domain::TrigramId::Qian, domain::TrigramId::Qian},
      {2, "坤为地", domain::TrigramId::Kun, domain::TrigramId::Kun},
      {3, "水雷屯", domain::TrigramId::Kan, domain::TrigramId::Zhen},
      {4, "山水蒙", domain::TrigramId::Gen, domain::TrigramId::Kan},
      {5, "水天需", domain::TrigramId::Kan, domain::TrigramId::Qian},
      {6, "天水讼", domain::TrigramId::Qian, domain::TrigramId::Kan},
      {7, "地水师", domain::TrigramId::Kun, domain::TrigramId::Kan},
      {8, "水地比", domain::TrigramId::Kan, domain::TrigramId::Kun},
      {9, "风天小畜", domain::TrigramId::Xun, domain::TrigramId::Qian},
      {10, "天泽履", domain::TrigramId::Qian, domain::TrigramId::Dui},
      {11, "地天泰", domain::TrigramId::Kun, domain::TrigramId::Qian},
      {12, "天地否", domain::TrigramId::Qian, domain::TrigramId::Kun},
      {13, "天火同人", domain::TrigramId::Qian, domain::TrigramId::Li},
      {14, "火天大有", domain::TrigramId::Li, domain::TrigramId::Qian},
      {15, "地山谦", domain::TrigramId::Kun, domain::TrigramId::Gen},
      {16, "雷地豫", domain::TrigramId::Zhen, domain::TrigramId::Kun},
      {17, "泽雷随", domain::TrigramId::Dui, domain::TrigramId::Zhen},
      {18, "山风蛊", domain::TrigramId::Gen, domain::TrigramId::Xun},
      {19, "地泽临", domain::TrigramId::Kun, domain::TrigramId::Dui},
      {20, "风地观", domain::TrigramId::Xun, domain::TrigramId::Kun},
      {21, "火雷噬嗑", domain::TrigramId::Li, domain::TrigramId::Zhen},
      {22, "山火贲", domain::TrigramId::Gen, domain::TrigramId::Li},
      {23, "山地剥", domain::TrigramId::Gen, domain::TrigramId::Kun},
      {24, "地雷复", domain::TrigramId::Kun, domain::TrigramId::Zhen},
      {25, "天雷无妄", domain::TrigramId::Qian, domain::TrigramId::Zhen},
      {26, "山天大畜", domain::TrigramId::Gen, domain::TrigramId::Qian},
      {27, "山雷颐", domain::TrigramId::Gen, domain::TrigramId::Zhen},
      {28, "泽风大过", domain::TrigramId::Dui, domain::TrigramId::Xun},
      {29, "坎为水", domain::TrigramId::Kan, domain::TrigramId::Kan},
      {30, "离为火", domain::TrigramId::Li, domain::TrigramId::Li},
      {31, "泽山咸", domain::TrigramId::Dui, domain::TrigramId::Gen},
      {32, "雷风恒", domain::TrigramId::Zhen, domain::TrigramId::Xun},
      {33, "天山遁", domain::TrigramId::Qian, domain::TrigramId::Gen},
      {34, "雷天大壮", domain::TrigramId::Zhen, domain::TrigramId::Qian},
      {35, "火地晋", domain::TrigramId::Li, domain::TrigramId::Kun},
      {36, "地火明夷", domain::TrigramId::Kun, domain::TrigramId::Li},
      {37, "风火家人", domain::TrigramId::Xun, domain::TrigramId::Li},
      {38, "火泽睽", domain::TrigramId::Li, domain::TrigramId::Dui},
      {39, "水山蹇", domain::TrigramId::Kan, domain::TrigramId::Gen},
      {40, "雷水解", domain::TrigramId::Zhen, domain::TrigramId::Kan},
      {41, "山泽损", domain::TrigramId::Gen, domain::TrigramId::Dui},
      {42, "风雷益", domain::TrigramId::Xun, domain::TrigramId::Zhen},
      {43, "泽天夬", domain::TrigramId::Dui, domain::TrigramId::Qian},
      {44, "天风姤", domain::TrigramId::Qian, domain::TrigramId::Xun},
      {45, "泽地萃", domain::TrigramId::Dui, domain::TrigramId::Kun},
      {46, "地风升", domain::TrigramId::Kun, domain::TrigramId::Xun},
      {47, "泽水困", domain::TrigramId::Dui, domain::TrigramId::Kan},
      {48, "水风井", domain::TrigramId::Kan, domain::TrigramId::Xun},
      {49, "泽火革", domain::TrigramId::Dui, domain::TrigramId::Li},
      {50, "火风鼎", domain::TrigramId::Li, domain::TrigramId::Xun},
      {51, "震为雷", domain::TrigramId::Zhen, domain::TrigramId::Zhen},
      {52, "艮为山", domain::TrigramId::Gen, domain::TrigramId::Gen},
      {53, "风山渐", domain::TrigramId::Xun, domain::TrigramId::Gen},
      {54, "雷泽归妹", domain::TrigramId::Zhen, domain::TrigramId::Dui},
      {55, "雷火丰", domain::TrigramId::Zhen, domain::TrigramId::Li},
      {56, "火山旅", domain::TrigramId::Li, domain::TrigramId::Gen},
      {57, "巽为风", domain::TrigramId::Xun, domain::TrigramId::Xun},
      {58, "兑为泽", domain::TrigramId::Dui, domain::TrigramId::Dui},
      {59, "风水涣", domain::TrigramId::Xun, domain::TrigramId::Kan},
      {60, "水泽节", domain::TrigramId::Kan, domain::TrigramId::Dui},
      {61, "风泽中孚", domain::TrigramId::Xun, domain::TrigramId::Dui},
      {62, "雷山小过", domain::TrigramId::Zhen, domain::TrigramId::Gen},
      {63, "水火既济", domain::TrigramId::Kan, domain::TrigramId::Li},
      {64, "火水未济", domain::TrigramId::Li, domain::TrigramId::Kan},
  }};
  return values;
}

const HexagramDefinition* findHexagram(domain::TrigramId upper,
                                       domain::TrigramId lower) noexcept {
  for (const auto& hexagram : allHexagrams()) {
    if (hexagram.upper == upper && hexagram.lower == lower) {
      return &hexagram;
    }
  }
  return nullptr;
}

const HexagramDefinition* findHexagramByNumber(
    std::uint8_t number) noexcept {
  if (number < 1 || number > 64) {
    return nullptr;
  }
  return &allHexagrams()[number - 1];
}

}  // namespace jinggua::data
