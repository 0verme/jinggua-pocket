#pragma once

#include "jinggua/application/state_machine.h"
#include "jinggua/hardware/display.h"

namespace jinggua::ui {

// Minimal offline history browser. Shows the record at the state machine's
// history cursor: serial, original hexagram, moving lines, and the changed
// hexagram when present. Full UX polish is tracked separately (#6).
void renderHistory(hardware::Display& display,
                   const application::StateMachine& stateMachine) noexcept;

}  // namespace jinggua::ui
