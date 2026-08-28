#include "jinggua/hardware/random.h"

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#else
#include <random>
#endif

namespace jinggua::hardware {

domain::CoinSide Esp32RandomProvider::tossCoin() {
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
  const auto entropy = esp_random();
#else
  static std::random_device entropySource;
  const auto entropy = entropySource();
#endif
  return (entropy & 1U) == 0U ? domain::CoinSide::Back
                               : domain::CoinSide::Front;
}

}  // namespace jinggua::hardware
