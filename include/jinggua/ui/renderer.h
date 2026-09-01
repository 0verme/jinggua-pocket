#pragma once

#include <cstdint>

#include "jinggua/application/state_machine.h"
#include "jinggua/hardware/display.h"
#include "jinggua/ui/coin_animation.h"

namespace jinggua::ui {

class Renderer final {
 public:
  explicit Renderer(hardware::Display& display) noexcept;

  void render(const application::StateMachine& stateMachine) noexcept;
  bool update(const application::StateMachine& stateMachine,
              std::uint32_t nowMs) noexcept;

  bool isDirty() const noexcept { return animationDirty_; }
  void acknowledgeRender() noexcept { animationDirty_ = false; }

 private:
  hardware::Display& display_;
  CoinAnimation coinAnimation_{};
  std::uint8_t animationLinePosition_{0};
  bool animationDirty_{false};
};

}  // namespace jinggua::ui
