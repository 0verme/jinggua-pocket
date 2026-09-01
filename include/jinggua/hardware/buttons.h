#pragma once

#include "jinggua/application/input_event.h"

namespace jinggua::hardware {

class StickS3Buttons final {
 public:
  void begin() noexcept;
  application::InputEvent poll() noexcept;

  // Discard the wake press and any hold/click generated while the button is
  // still held after light sleep. The next press is delivered normally.
  void ignoreUntilReleased() noexcept;

 private:
  bool ignoreUntilReleased_{false};
};

}  // namespace jinggua::hardware
