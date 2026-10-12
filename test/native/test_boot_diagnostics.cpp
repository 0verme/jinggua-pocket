#include <cstring>

#include "test_framework.h"

#include "jinggua/hardware/boot_diagnostics.h"

void runBootDiagnosticsTests(TestRunner& runner) {
  using jinggua::hardware::BootResetReason;
  using jinggua::hardware::bootResetReasonName;

  EXPECT(runner, std::strcmp(bootResetReasonName(BootResetReason::Unknown),
                             "unknown") == 0);
  EXPECT(runner, std::strcmp(bootResetReasonName(BootResetReason::PowerOn),
                             "power_on") == 0);
  EXPECT(runner, std::strcmp(bootResetReasonName(BootResetReason::External),
                             "external") == 0);
  EXPECT(runner, std::strcmp(bootResetReasonName(BootResetReason::Software),
                             "software") == 0);
  EXPECT(runner, std::strcmp(bootResetReasonName(BootResetReason::Panic),
                             "panic") == 0);
  EXPECT(runner,
         std::strcmp(bootResetReasonName(BootResetReason::InterruptWatchdog),
                     "interrupt_watchdog") == 0);
  EXPECT(runner,
         std::strcmp(bootResetReasonName(BootResetReason::TaskWatchdog),
                     "task_watchdog") == 0);
  EXPECT(runner, std::strcmp(bootResetReasonName(BootResetReason::Watchdog),
                             "watchdog") == 0);
  EXPECT(runner, std::strcmp(bootResetReasonName(BootResetReason::DeepSleep),
                             "deep_sleep") == 0);
  EXPECT(runner, std::strcmp(bootResetReasonName(BootResetReason::Brownout),
                             "brownout") == 0);
  EXPECT(runner, std::strcmp(bootResetReasonName(BootResetReason::Sdio),
                             "sdio") == 0);
  EXPECT(runner,
         std::strcmp(bootResetReasonName(static_cast<BootResetReason>(255U)),
                     "unknown") == 0);
}
