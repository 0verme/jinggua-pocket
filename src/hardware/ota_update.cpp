#include "jinggua/hardware/ota_update.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#ifndef JINGGUA_OTA_ROOT_CA
#define JINGGUA_OTA_ROOT_CA ""
#endif

#ifndef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 0
#endif
#endif

namespace jinggua::hardware {
namespace {

#if defined(ARDUINO)
constexpr char kFirmwareManifestUrl[] =
    "https://github.com/0verme/jinggua-pocket/releases/latest/"
    "download/firmware-manifest.txt";
constexpr std::uint32_t kHttpConnectTimeoutMs = 7000;
constexpr std::uint32_t kHttpReadTimeoutMs = 10000;
constexpr std::size_t kTransferChunkBytes = 1024;
constexpr std::uint32_t kUpdateTaskStackDepth = 8192;
constexpr std::size_t kMaxHttpRedirects = 3;

bool isConfigured() noexcept {
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
  return JINGGUA_OTA_ROOT_CA[0] != '\0';
#else
  return false;
#endif
}

bool configureTls(WiFiClientSecure& client) noexcept {
  if (!isConfigured()) {
    return false;
  }
  client.setCACert(JINGGUA_OTA_ROOT_CA);
  client.setHandshakeTimeout(kHttpConnectTimeoutMs / 1000U);
  return true;
}

bool isTrustedRedirectTarget(const String& url,
                             bool manifestRequest) noexcept {
  constexpr char kRepositoryReleasePrefix[] =
      "https://github.com/0verme/jinggua-pocket/releases/download/";
  constexpr char kManifestSuffix[] = "/firmware-manifest.txt";
  constexpr char kReleaseAssetsPrefix[] =
      "https://release-assets.githubusercontent.com/";
  constexpr char kObjectsPrefix[] = "https://objects.githubusercontent.com/";
  if (url.startsWith(kReleaseAssetsPrefix) ||
      url.startsWith(kObjectsPrefix)) {
    return true;
  }
  return manifestRequest && url.startsWith(kRepositoryReleasePrefix) &&
         url.endsWith(kManifestSuffix) && url.indexOf('?') < 0;
}

bool getWithTrustedRedirects(HTTPClient& http, WiFiClientSecure& tlsClient,
                             String& url, int& statusCode,
                             bool manifestRequest) noexcept {
  const char* headers[] = {"Location"};
  http.collectHeaders(headers, 1);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  for (std::size_t redirect = 0; redirect <= kMaxHttpRedirects; ++redirect) {
    if (!http.begin(tlsClient, url)) {
      statusCode = -1;
      return false;
    }
    statusCode = http.GET();
    const bool redirected = statusCode == HTTP_CODE_MOVED_PERMANENTLY ||
                            statusCode == HTTP_CODE_FOUND ||
                            statusCode == HTTP_CODE_SEE_OTHER ||
                            statusCode == HTTP_CODE_TEMPORARY_REDIRECT ||
                            statusCode == HTTP_CODE_PERMANENT_REDIRECT;
    if (!redirected) {
      return true;
    }

    const String nextUrl = http.getLocation();
    http.end();
    if (redirect >= kMaxHttpRedirects ||
        !isTrustedRedirectTarget(nextUrl, manifestRequest)) {
      return false;
    }
    url = nextUrl;
  }
  return false;
}

char hexDigit(std::uint8_t value) noexcept {
  return value < 10U ? static_cast<char>('0' + value)
                     : static_cast<char>('a' + value - 10U);
}

bool sha256Matches(const std::uint8_t* digest,
                   const char* expected) noexcept {
  if (digest == nullptr || expected == nullptr ||
      std::strlen(expected) != application::kFirmwareSha256HexLength) {
    return false;
  }
  for (std::size_t index = 0; index < 32; ++index) {
    const char high = hexDigit(static_cast<std::uint8_t>(digest[index] >> 4U));
    const char low = hexDigit(static_cast<std::uint8_t>(digest[index] & 0x0FU));
    if (expected[index * 2] != high || expected[index * 2 + 1] != low) {
      return false;
    }
  }
  return true;
}
#endif

}  // namespace

bool Esp32FirmwareUpdateClient::beginCheck() noexcept {
#if defined(ARDUINO)
  if (!isConfigured()) {
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::NotConfigured);
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::Offline);
    return false;
  }
  return startTask(Operation::Check);
#else
  return false;
#endif
}

bool Esp32FirmwareUpdateClient::beginInstall() noexcept {
#if defined(ARDUINO)
  return startTask(Operation::Install);
#else
  return false;
#endif
}

void Esp32FirmwareUpdateClient::cancel() noexcept {
#if defined(ARDUINO)
  portENTER_CRITICAL(&statusMux_);
  if (operationActive_) {
    cancelRequested_ = true;
  }
  portEXIT_CRITICAL(&statusMux_);
#endif
}

application::FirmwareUpdateStatus Esp32FirmwareUpdateClient::status()
    const noexcept {
#if defined(ARDUINO)
  portENTER_CRITICAL(&statusMux_);
  const auto result = status_;
  portEXIT_CRITICAL(&statusMux_);
  return result;
#else
  application::FirmwareUpdateStatus result;
  result.state = application::FirmwareUpdateState::Failed;
  result.error = application::FirmwareUpdateError::NotConfigured;
  return result;
#endif
}

#if defined(ARDUINO)

bool Esp32FirmwareUpdateClient::startTask(Operation operation) noexcept {
  portENTER_CRITICAL(&statusMux_);
  const bool allowed = !operationActive_ &&
      (operation == Operation::Check ||
       (operation == Operation::Install &&
        status_.state == application::FirmwareUpdateState::Available));
  if (!allowed) {
    portEXIT_CRITICAL(&statusMux_);
    return false;
  }

  operation_ = operation;
  operationActive_ = true;
  cancelRequested_ = false;
  status_.state = operation == Operation::Check
                      ? application::FirmwareUpdateState::Checking
                      : application::FirmwareUpdateState::Downloading;
  status_.error = application::FirmwareUpdateError::None;
  status_.progressPercent = 0;
  portEXIT_CRITICAL(&statusMux_);

  const auto taskCreated = xTaskCreate(
      &Esp32FirmwareUpdateClient::taskEntry, "jinggua-ota",
      kUpdateTaskStackDepth, this, 1, nullptr);
  if (taskCreated == pdPASS) {
    return true;
  }

  portENTER_CRITICAL(&statusMux_);
  operationActive_ = false;
  operation_ = Operation::None;
  status_.state = application::FirmwareUpdateState::Failed;
  status_.error = application::FirmwareUpdateError::ResourceUnavailable;
  portEXIT_CRITICAL(&statusMux_);
  return false;
}

bool Esp32FirmwareUpdateClient::isCancelled() const noexcept {
  portENTER_CRITICAL(&statusMux_);
  const bool cancelled = cancelRequested_;
  portEXIT_CRITICAL(&statusMux_);
  return cancelled;
}

void Esp32FirmwareUpdateClient::publish(
    application::FirmwareUpdateState state,
    application::FirmwareUpdateError error, std::uint8_t progress,
    const char* version) noexcept {
  portENTER_CRITICAL(&statusMux_);
  status_.state = state;
  status_.error = error;
  status_.progressPercent = std::min<std::uint8_t>(progress, 100U);
  if (version != nullptr) {
    const auto length = std::min<std::size_t>(
        std::strlen(version), application::kFirmwareVersionMaxLength);
    std::memcpy(status_.version, version, length);
    status_.version[length] = '\0';
  }
  portEXIT_CRITICAL(&statusMux_);
}

void Esp32FirmwareUpdateClient::taskEntry(void* context) noexcept {
  auto* client = static_cast<Esp32FirmwareUpdateClient*>(context);
  Operation operation;
  portENTER_CRITICAL(&client->statusMux_);
  operation = client->operation_;
  portEXIT_CRITICAL(&client->statusMux_);

  if (operation == Operation::Check) {
    client->runCheck();
  } else if (operation == Operation::Install) {
    client->runInstall();
  }

  portENTER_CRITICAL(&client->statusMux_);
  client->operationActive_ = false;
  client->operation_ = Operation::None;
  portEXIT_CRITICAL(&client->statusMux_);
  vTaskDelete(nullptr);
}

void Esp32FirmwareUpdateClient::runCheck() noexcept {
  WiFiClientSecure tlsClient;
  if (!configureTls(tlsClient)) {
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::NotConfigured);
    return;
  }

  HTTPClient http;
  http.setConnectTimeout(static_cast<std::int32_t>(kHttpConnectTimeoutMs));
  http.setTimeout(static_cast<std::uint16_t>(kHttpReadTimeoutMs));
  http.setReuse(false);
  http.useHTTP10(true);
  String manifestUrl(kFirmwareManifestUrl);
  int statusCode = -1;
  if (!getWithTrustedRedirects(http, tlsClient, manifestUrl, statusCode,
                               true) ||
      statusCode != HTTP_CODE_OK) {
    http.end();
    publish(application::FirmwareUpdateState::Failed,
            statusCode < 0 ? application::FirmwareUpdateError::Network
                           : application::FirmwareUpdateError::InvalidManifest);
    return;
  }

  std::array<char, application::kFirmwareManifestMaxBytes + 1> body{};
  std::size_t bodyLength = 0;
  const int contentLength = http.getSize();
  if (contentLength >
      static_cast<int>(application::kFirmwareManifestMaxBytes)) {
    http.end();
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::InvalidManifest);
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  const auto startedAt = millis();
  bool readFailed = stream == nullptr;
  while (!readFailed && bodyLength <= application::kFirmwareManifestMaxBytes) {
    if (isCancelled()) {
      http.end();
      publish(application::FirmwareUpdateState::Cancelled);
      return;
    }
    if (contentLength >= 0 &&
        bodyLength >= static_cast<std::size_t>(contentLength)) {
      break;
    }

    const int available = stream->available();
    if (available > 0) {
      const auto remaining = application::kFirmwareManifestMaxBytes - bodyLength;
      const auto requested = std::min<std::size_t>(
          remaining + 1U, static_cast<std::size_t>(available));
      const auto read = stream->readBytes(
          reinterpret_cast<std::uint8_t*>(body.data()) + bodyLength,
          requested);
      if (read == 0) {
        readFailed = true;
        break;
      }
      bodyLength += read;
      continue;
    }

    if (!stream->connected()) {
      break;
    }
    if (millis() - startedAt >= kHttpReadTimeoutMs) {
      readFailed = true;
      break;
    }
    delay(1);
  }
  http.end();

  if (bodyLength > application::kFirmwareManifestMaxBytes) {
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::InvalidManifest);
    return;
  }
  if (readFailed || bodyLength == 0 ||
      (contentLength >= 0 &&
       bodyLength != static_cast<std::size_t>(contentLength))) {
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::Network);
    return;
  }

  application::FirmwareUpdateManifest manifest;
  if (!application::parseFirmwareUpdateManifest(
          body.data(), bodyLength, manifest)) {
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::InvalidManifest);
    return;
  }
  int versionComparison = 0;
  if (!application::compareFirmwareVersions(
          manifest.version, application::kJingGuaFirmwareVersion,
          versionComparison)) {
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::InvalidVersion);
    return;
  }

  portENTER_CRITICAL(&statusMux_);
  manifest_ = manifest;
  portEXIT_CRITICAL(&statusMux_);
  if (versionComparison <= 0) {
    publish(application::FirmwareUpdateState::UpToDate,
            application::FirmwareUpdateError::None, 100, manifest.version);
  } else {
    publish(application::FirmwareUpdateState::Available,
            application::FirmwareUpdateError::None, 0, manifest.version);
  }
}

void Esp32FirmwareUpdateClient::runInstall() noexcept {
  application::FirmwareUpdateManifest manifest;
  portENTER_CRITICAL(&statusMux_);
  manifest = manifest_;
  portEXIT_CRITICAL(&statusMux_);

  const auto* updatePartition = esp_ota_get_next_update_partition(nullptr);
  if (updatePartition == nullptr || manifest.sizeBytes == 0 ||
      manifest.sizeBytes > updatePartition->size) {
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::NoSpace, 0, manifest.version);
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::Offline, 0, manifest.version);
    return;
  }

  WiFiClientSecure tlsClient;
  if (!configureTls(tlsClient)) {
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::NotConfigured, 0,
            manifest.version);
    return;
  }

  HTTPClient http;
  http.setConnectTimeout(static_cast<std::int32_t>(kHttpConnectTimeoutMs));
  http.setTimeout(static_cast<std::uint16_t>(kHttpReadTimeoutMs));
  http.setReuse(false);
  http.useHTTP10(true);
  String firmwareUrl(manifest.firmwareUrl);
  int statusCode = -1;
  if (!getWithTrustedRedirects(http, tlsClient, firmwareUrl, statusCode,
                               false) ||
      statusCode != HTTP_CODE_OK) {
    http.end();
    publish(application::FirmwareUpdateState::Failed,
            statusCode < 0 ? application::FirmwareUpdateError::Network
                           : application::FirmwareUpdateError::DownloadInterrupted,
            0, manifest.version);
    return;
  }
  const int contentLength = http.getSize();
  if (contentLength >= 0 &&
      static_cast<std::uint32_t>(contentLength) != manifest.sizeBytes) {
    http.end();
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::DownloadInterrupted, 0,
            manifest.version);
    return;
  }
  if (isCancelled()) {
    http.end();
    publish(application::FirmwareUpdateState::Cancelled,
            application::FirmwareUpdateError::None, 0, manifest.version);
    return;
  }

  if (!Update.begin(manifest.sizeBytes, U_FLASH)) {
    http.end();
    publish(application::FirmwareUpdateState::Failed,
            Update.getError() == UPDATE_ERROR_NO_PARTITION ||
                    Update.getError() == UPDATE_ERROR_SPACE
                ? application::FirmwareUpdateError::NoSpace
                : application::FirmwareUpdateError::ActivationFailed,
            0, manifest.version);
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  std::array<std::uint8_t, kTransferChunkBytes> buffer{};
  mbedtls_sha256_context hash;
  mbedtls_sha256_init(&hash);
  bool hashStarted = mbedtls_sha256_starts_ret(&hash, 0) == 0;
  bool downloadFailed = stream == nullptr || !hashStarted;
  std::uint32_t downloaded = 0;
  std::uint32_t lastDataAt = millis();

  while (!downloadFailed && downloaded < manifest.sizeBytes) {
    if (isCancelled()) {
      Update.abort();
      mbedtls_sha256_free(&hash);
      http.end();
      publish(application::FirmwareUpdateState::Cancelled,
              application::FirmwareUpdateError::None, 0, manifest.version);
      return;
    }

    const auto remaining = manifest.sizeBytes - downloaded;
    const int available = stream->available();
    if (available > 0) {
      const auto requested = std::min<std::size_t>(
          std::min<std::size_t>(buffer.size(), remaining),
          static_cast<std::size_t>(available));
      const auto read = stream->readBytes(buffer.data(), requested);
      if (read == 0 ||
          mbedtls_sha256_update_ret(&hash, buffer.data(), read) != 0 ||
          Update.write(buffer.data(), read) != read) {
        downloadFailed = true;
        break;
      }
      downloaded += static_cast<std::uint32_t>(read);
      lastDataAt = millis();
      const auto progress = static_cast<std::uint8_t>(
          (downloaded * 100U) / manifest.sizeBytes);
      publish(application::FirmwareUpdateState::Downloading,
              application::FirmwareUpdateError::None, progress,
              manifest.version);
      continue;
    }

    if (!stream->connected() || millis() - lastDataAt >= kHttpReadTimeoutMs) {
      downloadFailed = true;
      break;
    }
    delay(1);
  }

  std::array<std::uint8_t, 32> digest{};
  if (!downloadFailed &&
      mbedtls_sha256_finish_ret(&hash, digest.data()) != 0) {
    downloadFailed = true;
  }
  mbedtls_sha256_free(&hash);
  http.end();

  if (downloadFailed || downloaded != manifest.sizeBytes) {
    Update.abort();
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::DownloadInterrupted,
            static_cast<std::uint8_t>((downloaded * 100U) /
                                      manifest.sizeBytes),
            manifest.version);
    return;
  }
  if (!sha256Matches(digest.data(), manifest.sha256)) {
    Update.abort();
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::HashMismatch, 0,
            manifest.version);
    return;
  }
  if (!Update.end(false)) {
    Update.abort();
    publish(application::FirmwareUpdateState::Failed,
            application::FirmwareUpdateError::InvalidImage, 0,
            manifest.version);
    return;
  }

  publish(application::FirmwareUpdateState::Rebooting,
          application::FirmwareUpdateError::None, 100, manifest.version);
  vTaskDelay(pdMS_TO_TICKS(1200));
  ESP.restart();
}

#endif

bool verifyPendingFirmwareBoot(bool essentialServicesReady) noexcept {
#if defined(ARDUINO) && CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
  const auto* runningPartition = esp_ota_get_running_partition();
  if (runningPartition == nullptr) {
    return true;
  }
  esp_ota_img_states_t imageState;
  if (esp_ota_get_state_partition(runningPartition, &imageState) != ESP_OK ||
      imageState != ESP_OTA_IMG_PENDING_VERIFY) {
    return true;
  }

  if (!essentialServicesReady) {
    if (esp_ota_mark_app_invalid_rollback_and_reboot() != ESP_OK) {
      ESP.restart();
    }
    return false;
  }
  if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
    if (esp_ota_mark_app_invalid_rollback_and_reboot() != ESP_OK) {
      ESP.restart();
    }
    return false;
  }
  return true;
#else
  (void)essentialServicesReady;
  return true;
#endif
}

}  // namespace jinggua::hardware
