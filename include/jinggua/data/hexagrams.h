#pragma once

#include <array>
#include <cstdint>

#include "jinggua/domain/trigram.h"

namespace jinggua::data {

struct HexagramDefinition {
  std::uint8_t number;
  const char* name;
  domain::TrigramId upper;
  domain::TrigramId lower;
};

const std::array<HexagramDefinition, 64>& allHexagrams() noexcept;
const HexagramDefinition* findHexagram(domain::TrigramId upper,
                                       domain::TrigramId lower) noexcept;

// Looks up a hexagram by its King Wen number (1..64). Returns nullptr for
// out-of-range numbers so corrupt history records degrade gracefully.
const HexagramDefinition* findHexagramByNumber(
    std::uint8_t number) noexcept;

}  // namespace jinggua::data
