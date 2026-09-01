#pragma once

#include <cstddef>
#include <cstdint>

namespace jinggua::hardware {

extern const std::uint8_t kChineseFontData[];
extern const std::size_t kChineseFontDataSize;

constexpr std::uint16_t kChineseFontGlyphCount = 274;
constexpr std::size_t kChineseFontDataBytes = 38601;
constexpr std::uint8_t kChineseFontPixelSize = 12;
constexpr std::uint16_t kChineseFontWeight = 500;

}  // namespace jinggua::hardware
