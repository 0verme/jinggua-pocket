#pragma once

#include <cstdint>

namespace jinggua::ui::typography {

// The StickS3 display is 135x240.  These semantic sizes are shared by every
// screen; a page chooses a role, never a private setTextSize value.
constexpr std::uint8_t kTitle = 3;
constexpr std::uint8_t kSection = 2;
constexpr std::uint8_t kPrimary = 2;
constexpr std::uint8_t kSecondary = 1;
constexpr std::uint8_t kStatus = 1;
constexpr std::uint8_t kFooter = 1;
constexpr std::uint8_t kCoin = 2;

// Noto Sans SC glyphs are rasterized at 12 px; the generated VLW asset has a
// 16 px line box and M5GFX scales that same asset.
constexpr int kBaseLineHeight = 16;
constexpr int kTitleLineHeight = kBaseLineHeight * kTitle;
constexpr int kSectionLineHeight = kBaseLineHeight * kSection;
constexpr int kPrimaryLineHeight = kBaseLineHeight * kPrimary;
constexpr int kSecondaryLineHeight = kBaseLineHeight * kSecondary;

}  // namespace jinggua::ui::typography
