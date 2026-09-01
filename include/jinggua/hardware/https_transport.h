#pragma once

#include "jinggua/application/jinggua_api_client.h"

#if defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <array>
#endif

namespace jinggua::hardware {

// HTTPS adapter for the existing JingGua Web /api/divinations endpoint.
// Configuration is injected at build time; no credential is stored here.
// The one-shot request runs away from the UI loop and is polled by the
// application. It never retries or reconnects in the background.
class Esp32HttpsTransport final : public application::HttpTransport {
 public:
  static constexpr std::uint32_t kConnectTimeoutMs = 5000;
  static constexpr std::uint32_t kResponseTimeoutMs = 5000;

  bool beginPost(const char* payload, std::size_t payloadLength,
                 const char* idempotencyKey) noexcept override;
  application::TransportStatus poll(
      application::HttpResponse& response) noexcept override;

 private:
#if defined(ARDUINO)
  enum class OperationState : std::uint8_t {
    Idle,
    Pending,
    Ready,
  };

  static void taskEntry(void* context) noexcept;
  application::HttpResponse performPost(const char* payload,
                                         std::size_t payloadLength,
                                         const char* idempotencyKey) noexcept;

  std::array<char, application::kApiRequestMaxBytes + 1> payload_{};
  std::array<char, application::kApiRecordIdMaxLength + 1> idempotencyKey_{};
  std::size_t payloadLength_{0};
  application::HttpResponse response_{};
  volatile OperationState operationState_{OperationState::Idle};
  portMUX_TYPE criticalMux_ = portMUX_INITIALIZER_UNLOCKED;
#endif
};

}  // namespace jinggua::hardware
