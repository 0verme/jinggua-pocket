#pragma once

#include "jinggua/application/state_machine.h"
#include "jinggua/hardware/display.h"

namespace jinggua::ui {

class Renderer final {
 public:
  explicit Renderer(hardware::Display& display) noexcept;

  void render(const application::StateMachine& stateMachine) noexcept;

 private:
  hardware::Display& display_;
};

}  // namespace jinggua::ui
