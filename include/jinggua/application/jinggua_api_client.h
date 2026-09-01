#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "jinggua/application/divination_session.h"

namespace jinggua::application {

inline constexpr const char* kJingGuaApiVersion = "v1";
inline constexpr std::size_t kApiRequestMaxBytes = 512;
inline constexpr std::size_t kApiResponseMaxBytes = 512;
inline constexpr std::size_t kApiRecordIdMaxLength = 64;

enum class ApiError : std::uint8_t {
  None,
  Offline,
  WifiUnavailable,
  Timeout,
  TransportError,
  Http4xx,
  Http5xx,
  InvalidResponse,
  ApiVersionMismatch,
};

const char* apiErrorName(ApiError error) noexcept;

struct ApiResult {
  ApiError error{ApiError::None};
  bool accepted{false};
  bool complete{false};
  std::array<char, kApiRecordIdMaxLength + 1> recordId{};

  bool isPending() const noexcept { return !complete; }
  bool isSuccess() const noexcept {
    return complete && accepted && error == ApiError::None;
  }

  static ApiResult pending() noexcept;
  static ApiResult success(const char* recordId) noexcept;
  static ApiResult failure(ApiError error) noexcept;
};

enum class TransportStatus : std::uint8_t {
  Idle,
  Pending,
  Ok,
  Timeout,
  NetworkError,
  ResponseTooLarge,
  ConfigurationError,
};

// A bounded HTTP response. Implementations must never write more than
// kApiResponseMaxBytes bytes to body and must leave a trailing NUL when
// bodyLength is non-zero.
struct HttpResponse {
  TransportStatus transportStatus{TransportStatus::Idle};
  int statusCode{0};
  std::array<char, kApiResponseMaxBytes + 1> body{};
  std::size_t bodyLength{0};
};

// Application port for one user-triggered HTTP POST. beginPost() must only
// start/copy the request and return quickly; poll() advances a bounded
// one-shot operation. Native tests inject a fake transport here.
class HttpTransport {
 public:
  virtual ~HttpTransport() = default;

  virtual bool beginPost(const char* payload, std::size_t payloadLength,
                         const char* idempotencyKey) noexcept = 0;
  virtual TransportStatus poll(HttpResponse& response) noexcept = 0;
};

// Application port used by StateMachine. It receives a const session so an
// API request can only observe an already completed local result.
class ApiClient {
 public:
  virtual ~ApiClient() = default;

  virtual bool beginUpload(const DivinationSession& session,
                           std::uint32_t localRecordId) noexcept = 0;
  virtual ApiResult poll() noexcept = 0;
};

// Adapter from the local domain model to the existing JingGua Web
// /api/divinations contract. It does not generate randomness and does not
// mutate the session.
class JingGuaApiClient final : public ApiClient {
 public:
  explicit JingGuaApiClient(HttpTransport& transport) noexcept;

  bool beginUpload(const DivinationSession& session,
                   std::uint32_t localRecordId) noexcept override;
  ApiResult poll() noexcept override;

 private:
  HttpTransport& transport_;
  std::array<char, kApiRequestMaxBytes + 1> payload_{};
  std::array<char, kApiRecordIdMaxLength + 1> id_{};
  std::size_t payloadLength_{0};
  bool pending_{false};
  ApiResult terminalResult_{ApiResult::failure(ApiError::TransportError)};
};

}  // namespace jinggua::application
