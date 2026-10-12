#include "jinggua/hardware/boot_diagnostics.h"

namespace jinggua::hardware {

const char* bootResetReasonName(BootResetReason reason) noexcept {
  switch (reason) {
    case BootResetReason::Unknown:
      return "unknown";
    case BootResetReason::PowerOn:
      return "power_on";
    case BootResetReason::External:
      return "external";
    case BootResetReason::Software:
      return "software";
    case BootResetReason::Panic:
      return "panic";
    case BootResetReason::InterruptWatchdog:
      return "interrupt_watchdog";
    case BootResetReason::TaskWatchdog:
      return "task_watchdog";
    case BootResetReason::Watchdog:
      return "watchdog";
    case BootResetReason::DeepSleep:
      return "deep_sleep";
    case BootResetReason::Brownout:
      return "brownout";
    case BootResetReason::Sdio:
      return "sdio";
  }
  return "unknown";
}

}  // namespace jinggua::hardware
