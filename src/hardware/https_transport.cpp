#include "jinggua/hardware/https_transport.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef JINGGUA_API_URL
#define JINGGUA_API_URL ""
#endif

#ifndef JINGGUA_API_ROOT_CA
#define JINGGUA_API_ROOT_CA ""
#endif
#endif

namespace jinggua::hardware {

#if defined(ARDUINO)

bool Esp32HttpsTransport::beginPost(const char* payload,
                                    std::size_t payloadLength,
                                    const char* idempotencyKey) noexcept {
  if (payload == nullptr || payloadLength == 0 ||
      payloadLength > application::kApiRequestMaxBytes ||
      idempotencyKey == nullptr) {
    return false;
  }

  portENTER_CRITICAL(&criticalMux_);
  if (operationState_ != OperationState::Idle) {
    portEXIT_CRITICAL(&criticalMux_);
    return false;
  }
  std::memcpy(payload_.data(), payload, payloadLength);
  payload_[payloadLength] = '\0';
  payloadLength_ = payloadLength;
  const auto idLength = std::strlen(idempotencyKey);
  if (idLength == 0 || idLength >= idempotencyKey_.size()) {
    portEXIT_CRITICAL(&criticalMux_);
    return false;
  }
  std::memcpy(idempotencyKey_.data(), idempotencyKey, idLength);
  idempotencyKey_[idLength] = '\0';
  operationState_ = OperationState::Pending;
  portEXIT_CRITICAL(&criticalMux_);

  const auto taskCreated = xTaskCreate(
      &Esp32HttpsTransport::taskEntry, "jinggua-api", 8192, this, 1, nullptr);
  if (taskCreated == pdPASS) {
    return true;
  }

  portENTER_CRITICAL(&criticalMux_);
  response_ = application::HttpResponse{};
  response_.transportStatus = application::TransportStatus::NetworkError;
  operationState_ = OperationState::Ready;
  portEXIT_CRITICAL(&criticalMux_);
  return true;
}

application::TransportStatus Esp32HttpsTransport::poll(
    application::HttpResponse& response) noexcept {
  portENTER_CRITICAL(&criticalMux_);
  if (operationState_ == OperationState::Pending) {
    portEXIT_CRITICAL(&criticalMux_);
    return application::TransportStatus::Pending;
  }
  if (operationState_ == OperationState::Idle) {
    portEXIT_CRITICAL(&criticalMux_);
    return application::TransportStatus::Idle;
  }

  response = response_;
  operationState_ = OperationState::Idle;
  const auto status = response.transportStatus;
  portEXIT_CRITICAL(&criticalMux_);
  return status;
}

void Esp32HttpsTransport::taskEntry(void* context) noexcept {
  auto* transport = static_cast<Esp32HttpsTransport*>(context);
  const auto response = transport->performPost(
      transport->payload_.data(), transport->payloadLength_,
      transport->idempotencyKey_.data());

  portENTER_CRITICAL(&transport->criticalMux_);
  transport->response_ = response;
  transport->operationState_ = OperationState::Ready;
  portEXIT_CRITICAL(&transport->criticalMux_);
  vTaskDelete(nullptr);
}

application::HttpResponse Esp32HttpsTransport::performPost(
    const char* payload, std::size_t payloadLength,
    const char* idempotencyKey) noexcept {
  application::HttpResponse response;

  if (payload == nullptr || payloadLength == 0 ||
      payloadLength > application::kApiRequestMaxBytes ||
      idempotencyKey == nullptr) {
    response.transportStatus = application::TransportStatus::ConfigurationError;
    return response;
  }

  constexpr const char* kHttpsPrefix = "https://";
  constexpr std::size_t kHttpsPrefixLength = 8;
  const auto urlLength = std::strlen(JINGGUA_API_URL);
  const bool https = urlLength > kHttpsPrefixLength &&
                     std::strncmp(JINGGUA_API_URL, kHttpsPrefix,
                                  kHttpsPrefixLength) == 0;
  if (!https || JINGGUA_API_ROOT_CA[0] == '\0' ||
      WiFi.status() != WL_CONNECTED) {
    response.transportStatus =
        JINGGUA_API_ROOT_CA[0] == '\0' || !https
            ? application::TransportStatus::ConfigurationError
            : application::TransportStatus::NetworkError;
    return response;
  }

  WiFiClientSecure tlsClient;
  // Certificate verification is intentionally enabled. There is no
  // setInsecure() fallback when the CA is absent; the request fails closed.
  tlsClient.setCACert(JINGGUA_API_ROOT_CA);
  tlsClient.setHandshakeTimeout(kConnectTimeoutMs / 1000U);

  HTTPClient http;
  http.setConnectTimeout(static_cast<int32_t>(kConnectTimeoutMs));
  http.setTimeout(static_cast<std::uint16_t>(kResponseTimeoutMs));
  http.setReuse(false);
  http.useHTTP10(true);
  if (!http.begin(tlsClient, String(JINGGUA_API_URL))) {
    response.transportStatus = application::TransportStatus::NetworkError;
    return response;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Idempotency-Key", String(idempotencyKey));

  const int statusCode = http.POST(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(payload)),
      payloadLength);
  if (statusCode < 0) {
    response.transportStatus =
        statusCode == HTTPC_ERROR_READ_TIMEOUT
            ? application::TransportStatus::Timeout
            : application::TransportStatus::NetworkError;
    http.end();
    return response;
  }

  response.transportStatus = application::TransportStatus::Ok;
  response.statusCode = statusCode;
  WiFiClient* stream = http.getStreamPtr();
  if (stream == nullptr) {
    response.transportStatus = application::TransportStatus::NetworkError;
    http.end();
    return response;
  }

  const int contentLength = http.getSize();
  if (contentLength > static_cast<int>(application::kApiResponseMaxBytes)) {
    response.transportStatus = application::TransportStatus::ResponseTooLarge;
    http.end();
    return response;
  }

  const auto startedAt = millis();
  while (true) {
    if (contentLength >= 0 &&
        response.bodyLength >= static_cast<std::size_t>(contentLength)) {
      break;
    }

    const int available = stream->available();
    if (available > 0) {
      if (response.bodyLength >= application::kApiResponseMaxBytes) {
        response.transportStatus = application::TransportStatus::ResponseTooLarge;
        break;
      }
      const auto room = application::kApiResponseMaxBytes - response.bodyLength;
      const auto requested = std::min(room, static_cast<std::size_t>(available));
      const auto read = stream->readBytes(
          reinterpret_cast<std::uint8_t*>(response.body.data()) +
              response.bodyLength,
          requested);
      if (read == 0) {
        response.transportStatus = application::TransportStatus::NetworkError;
        break;
      }
      response.bodyLength += read;
      continue;
    }

    if (!stream->connected()) {
      break;
    }
    if (millis() - startedAt >= kResponseTimeoutMs) {
      response.transportStatus = application::TransportStatus::Timeout;
      break;
    }
    delay(1);
  }

  if (response.transportStatus == application::TransportStatus::Ok &&
      contentLength >= 0 &&
      response.bodyLength < static_cast<std::size_t>(contentLength)) {
    response.transportStatus = application::TransportStatus::NetworkError;
  }
  if (response.transportStatus == application::TransportStatus::Ok) {
    response.body[response.bodyLength] = '\0';
  }
  http.end();
  return response;
}

#else

bool Esp32HttpsTransport::beginPost(const char* payload,
                                    std::size_t payloadLength,
                                    const char* idempotencyKey) noexcept {
  (void)payload;
  (void)payloadLength;
  (void)idempotencyKey;
  return false;
}

application::TransportStatus Esp32HttpsTransport::poll(
    application::HttpResponse& response) noexcept {
  response = application::HttpResponse{};
  response.transportStatus = application::TransportStatus::NetworkError;
  return application::TransportStatus::NetworkError;
}

#endif

}  // namespace jinggua::hardware
