#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "jinggua/domain/yao.h"

namespace jinggua::domain {

enum class TrigramId : std::uint8_t {
  Qian,
  Dui,
  Li,
  Zhen,
  Xun,
  Kan,
  Gen,
  Kun,
};

using TrigramLineSet = std::array<YinYang, 3>;

struct Trigram {
  TrigramId id;
  TrigramLineSet lines;
  const char* name;
  std::uint8_t binaryValue;
};

std::optional<Trigram> trigramFromLines(
    const TrigramLineSet& lines) noexcept;
const char* trigramName(TrigramId id) noexcept;

}  // namespace jinggua::domain
