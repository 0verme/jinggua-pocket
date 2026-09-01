#pragma once

#include <cstdint>

#include "jinggua/application/power_manager.h"

namespace jinggua::application {

enum class WakeReason : std::uint8_t {
  Button,
  Other,
  Unavailable,
};

const char* wakeReasonName(WakeReason reason) noexcept;

// Hardware port for applying a logical power state and entering light sleep.
// The application tests use PowerManager without this platform dependency.
class PowerHardware {
 public:
  virtual ~PowerHardware() = default;

  virtual void apply(PowerState state, std::uint8_t brightness) noexcept = 0;
  // Returns after light sleep exits. No reset/reinitialization is expected.
  virtual WakeReason enterLightSleep() noexcept = 0;
};

}  // namespace jinggua::application
