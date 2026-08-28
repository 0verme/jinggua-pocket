#pragma once

#include "jinggua/application/state_machine.h"
#include "jinggua/hardware/display.h"

namespace jinggua::ui {

void renderWelcome(hardware::Display& display) noexcept;
void renderPrepare(hardware::Display& display) noexcept;
void renderCasting(hardware::Display& display,
                   const application::DivinationSession& session) noexcept;
void renderLineResult(hardware::Display& display,
                      const application::DivinationSession& session) noexcept;
void renderHexagramResult(
    hardware::Display& display,
    const application::DivinationSession& session) noexcept;
void renderTransformedResult(
    hardware::Display& display,
    const application::DivinationSession& session) noexcept;
void renderResetConfirm(hardware::Display& display) noexcept;

}  // namespace jinggua::ui
