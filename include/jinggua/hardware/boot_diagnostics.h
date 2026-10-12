#pragma once

#include <cstdint>

namespace jinggua::hardware {

enum class BootResetReason : std::uint8_t {
  Unknown,
  PowerOn,
  External,
  Software,
  Panic,
  InterruptWatchdog,
  TaskWatchdog,
  Watchdog,
  DeepSleep,
  Brownout,
  Sdio,
};

const char* bootResetReasonName(BootResetReason reason) noexcept;

}  // namespace jinggua::hardware
