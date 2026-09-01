#include "jinggua/application/power_hardware.h"

namespace jinggua::application {

const char* wakeReasonName(WakeReason reason) noexcept {
  switch (reason) {
    case WakeReason::Button:
      return "button";
    case WakeReason::Other:
      return "other";
    case WakeReason::Unavailable:
      return "unavailable";
  }
  return "unknown";
}

}  // namespace jinggua::application
