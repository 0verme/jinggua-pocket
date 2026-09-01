#pragma once

#include <cstdint>

#include "jinggua/application/wifi_controller.h"

namespace jinggua::hardware {

// StickS3 / ESP32-S3 Wi-Fi adapter. Offline-first: the radio stays Off until
// the user explicitly calls enable(). Connection is non-blocking and advances
// through update() from the main loop; the firmware never auto-connects and
// never silently retries after a failure or timeout.
class Esp32WifiManager final : public application::WifiController {
 public:
  // Connection attempt window. After this many milliseconds the controller
  // reports Timeout and stops trying without user action.
  static constexpr std::uint32_t kConnectionTimeoutMs = 15000;

  Esp32WifiManager() noexcept = default;

  // Initialize the radio in the guaranteed-off boot state. This is called
  // from setup(), not from the global constructor, so Arduino is ready.
  void begin() noexcept;

  application::WifiState state() const noexcept override;
  bool enable() noexcept override;
  void disable() noexcept override;
  application::WifiState update(std::uint32_t nowMs) noexcept override;
  const char* ssid() const noexcept override;
  bool configured() const noexcept override;

 private:
  application::WifiState state_{application::WifiState::Off};
  std::uint32_t startedAtMs_{0};
};

}  // namespace jinggua::hardware
