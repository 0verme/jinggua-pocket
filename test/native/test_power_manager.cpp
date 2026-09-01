#include <cstdint>
#include <limits>

#include "test_framework.h"

#include "jinggua/application/power_manager.h"

void runPowerManagerTests(TestRunner& runner) {
  using jinggua::application::PowerManager;
  using jinggua::application::PowerPolicy;
  using jinggua::application::PowerState;

  // A. Startup always begins Active.
  {
    PowerManager manager;
    EXPECT_EQ(runner, manager.state(), PowerState::Active);
    manager.begin(0U);
    EXPECT_EQ(runner, manager.state(), PowerState::Active);
    EXPECT_EQ(runner, manager.brightness(), PowerPolicy::kActiveBrightness);
  }

  // B/C. Dim begins exactly at 30 seconds, not before.
  {
    PowerManager manager;
    manager.begin(0U);
    EXPECT(runner, !manager.update(29'900U, true));
    EXPECT_EQ(runner, manager.state(), PowerState::Active);
    EXPECT(runner, manager.update(PowerPolicy::kDimAfterMs, true));
    EXPECT_EQ(runner, manager.state(), PowerState::Dim);
    EXPECT_EQ(runner, manager.brightness(), PowerPolicy::kDimBrightness);
  }

  // D. Display Off begins at 60 seconds and remains distinct from Dim.
  {
    PowerManager manager;
    manager.begin(0U);
    manager.update(PowerPolicy::kDimAfterMs, true);
    EXPECT(runner, manager.update(PowerPolicy::kDisplayOffAfterMs, true));
    EXPECT_EQ(runner, manager.state(), PowerState::DisplayOff);
    EXPECT_EQ(runner, manager.brightness(), static_cast<std::uint8_t>(0U));
    EXPECT_EQ(runner, manager.imuPollIntervalMs(),
              PowerPolicy::kDisplayOffImuPollIntervalMs);
  }

  // E. Light Sleep is a one-shot request at the configured five-minute mark.
  {
    PowerManager manager;
    manager.begin(0U);
    manager.update(PowerPolicy::kLightSleepAfterMs, true);
    EXPECT_EQ(runner, manager.state(), PowerState::LightSleep);
    EXPECT(runner, manager.lightSleepRequested());
    EXPECT(runner, manager.consumeLightSleepRequest());
    EXPECT(runner, !manager.lightSleepRequested());
    EXPECT(runner, !manager.imuPollingAllowed());
    manager.wake(PowerPolicy::kLightSleepAfterMs + 1U);
    EXPECT_EQ(runner, manager.state(), PowerState::Active);
    EXPECT_EQ(runner, manager.lastActivityAt(),
              PowerPolicy::kLightSleepAfterMs + 1U);
    EXPECT(runner, manager.imuPollingAllowed());
  }

  // F. A button/activity event wakes Dim and starts a fresh inactivity epoch.
  {
    PowerManager manager;
    manager.begin(0U);
    manager.update(PowerPolicy::kDimAfterMs, true);
    EXPECT_EQ(runner, manager.state(), PowerState::Dim);
    EXPECT(runner, manager.recordActivity(31'000U));
    EXPECT_EQ(runner, manager.state(), PowerState::Active);
    EXPECT_EQ(runner, manager.lastActivityAt(), 31'000U);
    EXPECT(runner, !manager.update(60'999U, true));
    EXPECT_EQ(runner, manager.state(), PowerState::Active);
    EXPECT(runner, manager.update(61'000U, true));
    EXPECT_EQ(runner, manager.state(), PowerState::Dim);
  }

  // G. A valid Shake is also activity while the display is off.
  {
    PowerManager manager;
    manager.begin(0U);
    manager.update(PowerPolicy::kDisplayOffAfterMs, true);
    EXPECT_EQ(runner, manager.state(), PowerState::DisplayOff);
    EXPECT(runner, manager.recordActivity(61'000U));
    EXPECT_EQ(runner, manager.state(), PowerState::Active);
  }

  // H/I. Animation and Wi-Fi Connecting inhibit automatic sleep.
  {
    PowerManager manager;
    manager.begin(0U);
    EXPECT(runner, !manager.update(PowerPolicy::kLightSleepAfterMs, false));
    EXPECT_EQ(runner, manager.state(), PowerState::Active);
    EXPECT(runner, !manager.lightSleepRequested());

    manager.update(PowerPolicy::kDimAfterMs, true);
    EXPECT_EQ(runner, manager.state(), PowerState::Dim);
    EXPECT(runner, manager.update(PowerPolicy::kLightSleepAfterMs, false));
    EXPECT_EQ(runner, manager.state(), PowerState::Active);
    EXPECT(runner, !manager.lightSleepRequested());
  }

  // J. Unsigned elapsed arithmetic remains correct across millis() wrap.
  {
    PowerManager manager;
    const auto start = std::numeric_limits<std::uint32_t>::max() - 1'000U;
    manager.begin(start);
    EXPECT(runner, !manager.update(start + 29'999U, true));
    EXPECT_EQ(runner, manager.state(), PowerState::Active);
    EXPECT(runner, manager.update(start + PowerPolicy::kDimAfterMs, true));
    EXPECT_EQ(runner, manager.state(), PowerState::Dim);
    EXPECT(runner,
           manager.update(start + PowerPolicy::kDisplayOffAfterMs, true));
    EXPECT_EQ(runner, manager.state(), PowerState::DisplayOff);
    EXPECT(runner,
           manager.update(start + PowerPolicy::kLightSleepAfterMs, true));
    EXPECT_EQ(runner, manager.state(), PowerState::LightSleep);
  }
}
