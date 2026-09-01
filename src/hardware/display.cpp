#include "jinggua/hardware/display.h"

#if defined(ARDUINO)
#include <M5Unified.h>

#include "jinggua/hardware/font_data.h"
#endif

namespace jinggua::hardware {

bool Display::begin() noexcept {
#if defined(ARDUINO)
  auto config = M5.config();
  config.serial_baudrate = 115200;
#if defined(JINGGUA_ENABLE_MIC_RESEARCH) && JINGGUA_ENABLE_MIC_RESEARCH
  config.internal_mic = true;
#else
  config.internal_mic = false;
#endif
  config.internal_spk = false;
  config.output_power = false;
  M5.begin(config);
  ready_ = true;
  fontReady_ = M5.Display.loadFont(kChineseFontData);
#else
  ready_ = true;
#endif
  return ready_;
}

void Display::clear(Color color) noexcept {
#if defined(ARDUINO)
  if (ready_) {
    M5.Display.fillScreen(color);
  }
#else
  (void)color;
#endif
}

void Display::drawText(const char* text, int x, int y, Color color,
                       std::uint8_t textSize) noexcept {
#if defined(ARDUINO)
  if (ready_) {
    if (!fontReady_) {
      // Keep the built-in Latin font as a last-resort fallback if a font asset
      // is corrupted or cannot be allocated during hardware initialization.
      // Do not reference the full built-in Chinese font: that would add 158 KB
      // back to the firmware even though the fallback is rarely used.
      M5.Display.setFont(&fonts::Font0);
    }
    M5.Display.setTextColor(color);
    M5.Display.setTextSize(textSize);
    M5.Display.drawString(text, x, y);
  }
#else
  (void)text;
  (void)x;
  (void)y;
  (void)color;
  (void)textSize;
#endif
}

void Display::drawLine(int x0, int y0, int x1, int y1,
                       Color color) noexcept {
#if defined(ARDUINO)
  if (ready_) {
    M5.Display.drawLine(x0, y0, x1, y1, color);
  }
#else
  (void)x0;
  (void)y0;
  (void)x1;
  (void)y1;
  (void)color;
#endif
}

void Display::drawEllipse(int centerX, int centerY, int radiusX, int radiusY,
                          Color color) noexcept {
#if defined(ARDUINO)
  if (ready_) {
    M5.Display.drawEllipse(centerX, centerY, radiusX, radiusY, color);
  }
#else
  (void)centerX;
  (void)centerY;
  (void)radiusX;
  (void)radiusY;
  (void)color;
#endif
}

void Display::fillEllipse(int centerX, int centerY, int radiusX, int radiusY,
                          Color color) noexcept {
#if defined(ARDUINO)
  if (ready_) {
    M5.Display.fillEllipse(centerX, centerY, radiusX, radiusY, color);
  }
#else
  (void)centerX;
  (void)centerY;
  (void)radiusX;
  (void)radiusY;
  (void)color;
#endif
}

int Display::width() const noexcept {
#if defined(ARDUINO)
  return ready_ ? M5.Display.width() : 135;
#else
  return 135;
#endif
}

int Display::height() const noexcept {
#if defined(ARDUINO)
  return ready_ ? M5.Display.height() : 240;
#else
  return 240;
#endif
}

}  // namespace jinggua::hardware
