#pragma once

#include <cstdint>

#include "jinggua/application/power_hardware.h"
#include "jinggua/hardware/display.h"

namespace jinggua::hardware {

// StickS3 adapter for the application power port. GPIO11/GPIO12 are the
// M5Unified BtnA/BtnB inputs and are configured as active-low light-sleep wake
// sources only for the duration of the sleep attempt.
class StickS3PowerController final : public application::PowerHardware {
 public:
  explicit StickS3PowerController(Display& display) noexcept
      : display_(display) {}

  void apply(application::PowerState state,
             std::uint8_t brightness) noexcept override;
  application::WakeReason enterLightSleep() noexcept override;

 private:
  Display& display_;
};

}  // namespace jinggua::hardware
