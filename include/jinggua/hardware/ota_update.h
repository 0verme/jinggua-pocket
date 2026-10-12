#pragma once

#include "jinggua/application/ota_update.h"

#if defined(ARDUINO)
#include <freertos/FreeRTOS.h>

#include <array>
#include <cstddef>
#include <cstdint>
#endif

namespace jinggua::hardware {

// HTTPS GitHub Releases client. It is inert unless the OTA CA is supplied at
// build time and the selected ESP-IDF bootloader supports rollback.
class Esp32FirmwareUpdateClient final
    : public application::FirmwareUpdateClient {
 public:
  bool beginCheck() noexcept override;
  bool beginInstall() noexcept override;
  void cancel() noexcept override;
  application::FirmwareUpdateStatus status() const noexcept override;

 private:
#if defined(ARDUINO)
  enum class Operation : std::uint8_t { None, Check, Install };

  static void taskEntry(void* context) noexcept;
  void runCheck() noexcept;
  void runInstall() noexcept;
  bool isCancelled() const noexcept;
  bool beginFinalization() noexcept;
  void publish(application::FirmwareUpdateState state,
               application::FirmwareUpdateError error =
                   application::FirmwareUpdateError::None,
               std::uint8_t progress = 0,
               const char* version = nullptr) noexcept;
  bool startTask(Operation operation) noexcept;

  mutable portMUX_TYPE statusMux_ = portMUX_INITIALIZER_UNLOCKED;
  application::FirmwareUpdateStatus status_{};
  application::FirmwareUpdateManifest manifest_{};
  Operation operation_{Operation::None};
  bool operationActive_{false};
  bool cancelRequested_{false};
#endif
};

// Called after essential application startup. Pending OTA images are marked
// valid only after the display and state machine initialize; otherwise the
// bootloader rollback path is requested.
bool verifyPendingFirmwareBoot(bool essentialServicesReady) noexcept;

}  // namespace jinggua::hardware
