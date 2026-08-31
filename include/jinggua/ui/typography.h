#pragma once

#include <cstdint>

namespace jinggua::ui::typography {

// The StickS3 display is 135x240. Keep the hierarchy semantic so screen
// layouts do not grow a second set of hard-coded font sizes.
constexpr std::uint8_t kTitle = 3;
constexpr std::uint8_t kSection = 2;
constexpr std::uint8_t kResult = 2;
constexpr std::uint8_t kBody = 1;
constexpr std::uint8_t kAuxiliary = 1;
constexpr std::uint8_t kFooter = 1;

}  // namespace jinggua::ui::typography
