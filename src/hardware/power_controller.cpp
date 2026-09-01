#include "jinggua/hardware/power_controller.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#endif

namespace jinggua::hardware {
namespace {

#if defined(ARDUINO)
constexpr gpio_num_t kButtonAPin = GPIO_NUM_11;
constexpr gpio_num_t kButtonBPin = GPIO_NUM_12;
constexpr std::uint64_t kButtonWakeMask =
    (1ULL << 11U) | (1ULL << 12U);

void disableButtonWake() noexcept {
  (void)gpio_wakeup_disable(kButtonAPin);
  (void)gpio_wakeup_disable(kButtonBPin);
  (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
}
#endif

}  // namespace

void StickS3PowerController::apply(application::PowerState state,
                                    std::uint8_t brightness) noexcept {
  switch (state) {
    case application::PowerState::Active:
    case application::PowerState::Dim:
      display_.displayWake();
      display_.setBrightness(brightness);
      break;
    case application::PowerState::DisplayOff:
    case application::PowerState::LightSleep:
      // LightSleep is prepared here; enterLightSleep() performs the MCU
      // transition after the display is already dark.
      display_.displayOff();
      break;
  }
}

application::WakeReason StickS3PowerController::enterLightSleep() noexcept {
  display_.displayOff();

#if !defined(ARDUINO)
  return application::WakeReason::Unavailable;
#else
  // M5Unified leaves BtnA/BtnB as inputs and the StickS3 buttons are
  // active-low. GPIO wake works for ESP32-S3 light sleep without changing
  // the application's ordinary button polling path.
  (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  const auto buttonAResult =
      gpio_wakeup_enable(kButtonAPin, GPIO_INTR_LOW_LEVEL);
  const auto buttonBResult =
      gpio_wakeup_enable(kButtonBPin, GPIO_INTR_LOW_LEVEL);
  const auto globalResult = esp_sleep_enable_gpio_wakeup();
  if (buttonAResult != ESP_OK || buttonBResult != ESP_OK ||
      globalResult != ESP_OK) {
    Serial.println("[Power] light sleep wake setup failed");
    disableButtonWake();
    return application::WakeReason::Unavailable;
  }

  const auto sleepResult = esp_light_sleep_start();
  const auto wakeCause = esp_sleep_get_wakeup_cause();
#if defined(SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP) && \
    SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
  const auto wakeMask = esp_sleep_get_gpio_wakeup_status();
#else
  const std::uint64_t wakeMask = 0U;
#endif
  disableButtonWake();

  if (sleepResult != ESP_OK) {
    Serial.println("[Power] light sleep did not start");
    return application::WakeReason::Unavailable;
  }

  if (wakeCause == ESP_SLEEP_WAKEUP_GPIO &&
      (wakeMask == 0U || (wakeMask & kButtonWakeMask) != 0U)) {
    return application::WakeReason::Button;
  }
  return application::WakeReason::Other;
#endif
}

}  // namespace jinggua::hardware
