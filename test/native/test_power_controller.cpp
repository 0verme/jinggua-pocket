#include <cstdint>

#include "test_framework.h"

#include "jinggua/application/power_hardware.h"
#include "jinggua/application/power_manager.h"
#include "jinggua/hardware/display.h"
#include "jinggua/hardware/power_controller.h"

void runPowerControllerTests(TestRunner& runner) {
  using jinggua::application::PowerState;
  using jinggua::application::WakeReason;
  jinggua::hardware::Display display;
  jinggua::hardware::StickS3PowerController controller(display);

  EXPECT(runner, display.begin());
  controller.apply(PowerState::Active, 128U);
  EXPECT(runner, !display.isDisplayOff());
  EXPECT_EQ(runner, display.brightness(), static_cast<std::uint8_t>(128U));

  controller.apply(PowerState::Dim, 32U);
  EXPECT(runner, !display.isDisplayOff());
  EXPECT_EQ(runner, display.brightness(), static_cast<std::uint8_t>(32U));

  controller.apply(PowerState::DisplayOff, 0U);
  EXPECT(runner, display.isDisplayOff());
  controller.apply(PowerState::LightSleep, 0U);
  EXPECT(runner, display.isDisplayOff());

  controller.apply(PowerState::Active, 128U);
  EXPECT(runner, !display.isDisplayOff());
  EXPECT_EQ(runner, display.brightness(), static_cast<std::uint8_t>(128U));

  // Native builds deliberately do not fake a sleep cycle or wake reason.
  EXPECT_EQ(runner, controller.enterLightSleep(), WakeReason::Unavailable);
}
