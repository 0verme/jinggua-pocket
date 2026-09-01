#pragma once

#include <cstdint>

namespace jinggua::hardware {

using Color = std::uint16_t;

class Display final {
 public:
  bool begin() noexcept;
  bool isReady() const noexcept { return ready_; }
  bool isFontReady() const noexcept { return fontReady_; }

  void setBrightness(std::uint8_t brightness) noexcept;
  void displayOff() noexcept;
  void displayWake() noexcept;
  std::uint8_t brightness() const noexcept { return brightness_; }
  bool isDisplayOff() const noexcept { return displayOff_; }

  void clear(Color color) noexcept;
  void drawText(const char* text, int x, int y, Color color,
                std::uint8_t textSize = 1) noexcept;
  void drawLine(int x0, int y0, int x1, int y1, Color color) noexcept;
  void drawEllipse(int centerX, int centerY, int radiusX, int radiusY,
                   Color color) noexcept;
  void fillEllipse(int centerX, int centerY, int radiusX, int radiusY,
                   Color color) noexcept;

  int width() const noexcept;
  int height() const noexcept;

 private:
  static constexpr std::uint8_t kDefaultBrightness = 128U;

  bool ready_{false};
  bool fontReady_{false};
  std::uint8_t brightness_{kDefaultBrightness};
  bool displayOff_{false};
};

}  // namespace jinggua::hardware
