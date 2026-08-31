#include "jinggua/application/wifi_controller.h"

namespace jinggua::application {

const char* wifiStateName(WifiState state) noexcept {
  switch (state) {
    case WifiState::Off:
      return "OFF";
    case WifiState::Connecting:
      return "CONNECTING";
    case WifiState::Connected:
      return "CONNECTED";
    case WifiState::Failed:
      return "FAILED";
    case WifiState::Timeout:
      return "TIMEOUT";
  }
  return "UNKNOWN";
}

}  // namespace jinggua::application
