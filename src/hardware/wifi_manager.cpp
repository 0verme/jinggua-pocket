#include "jinggua/hardware/wifi_manager.h"

#if defined(ARDUINO)
#include <WiFi.h>
#endif

// ----- DEFAULT CREDENTIALS (development-only placeholder) --------------------
// Override these via platformio.ini build flags from environment variables:
//   -DJINGGUA_WIFI_SSID="\"${sysenv.JINGGUA_WIFI_SSID}\""
//   -DJINGGUA_WIFI_PASSWORD="\"${sysenv.JINGGUA_WIFI_PASSWORD}\""
// When both are empty the adapter reports "not configured" and connecting is
// blocked.  No real credentials are ever committed to the repository.
// ----------------------------------------------------------------------------
#ifndef JINGGUA_WIFI_SSID
#define JINGGUA_WIFI_SSID ""
#endif

#ifndef JINGGUA_WIFI_PASSWORD
#define JINGGUA_WIFI_PASSWORD ""
#endif

namespace jinggua::hardware {
namespace {

constexpr const char* kSsid = JINGGUA_WIFI_SSID;
constexpr const char* kPassword = JINGGUA_WIFI_PASSWORD;

}  // namespace

void Esp32WifiManager::begin() noexcept {
#if defined(ARDUINO)
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
#endif
  state_ = application::WifiState::Off;
}

application::WifiState Esp32WifiManager::state() const noexcept {
  return state_;
}

bool Esp32WifiManager::configured() const noexcept {
  return kSsid[0] != '\0';
}

const char* Esp32WifiManager::ssid() const noexcept {
  return kSsid;
}

bool Esp32WifiManager::enable() noexcept {
  if (state_ == application::WifiState::Connecting ||
      state_ == application::WifiState::Connected) {
    return false;
  }
  if (!configured()) {
    state_ = application::WifiState::Failed;
    return false;
  }
#if defined(ARDUINO)
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  Serial.println("[WiFi] user requested connection");
  Serial.println("[WiFi] connecting");
  WiFi.begin(kSsid, kPassword);
  startedAtMs_ = millis();
  state_ = application::WifiState::Connecting;
  return true;
#else
  // No WiFi hardware on the native test host.
  state_ = application::WifiState::Failed;
  return false;
#endif
}

void Esp32WifiManager::disable() noexcept {
#if defined(ARDUINO)
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.println("[WiFi] disabled by user");
#endif
  state_ = application::WifiState::Off;
}

application::WifiState Esp32WifiManager::update(std::uint32_t nowMs) noexcept {
  if (state_ != application::WifiState::Connecting) {
    return state_;
  }
#if defined(ARDUINO)
  switch (WiFi.status()) {
    case WL_CONNECTED:
      state_ = application::WifiState::Connected;
      Serial.println("[WiFi] connected");
      break;
    case WL_CONNECT_FAILED:
    case WL_NO_SSID_AVAIL:
    case WL_CONNECTION_LOST:
      state_ = application::WifiState::Failed;
      Serial.println("[WiFi] failed");
      break;
    default:
      if (nowMs - startedAtMs_ >= kConnectionTimeoutMs) {
        WiFi.disconnect();
        state_ = application::WifiState::Timeout;
        Serial.println("[WiFi] timeout");
      }
      break;
  }
#else
  (void)nowMs;
  state_ = application::WifiState::Failed;
#endif
  return state_;
}

}  // namespace jinggua::hardware
