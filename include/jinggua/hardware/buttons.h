#pragma once

#include "jinggua/application/input_event.h"

namespace jinggua::hardware {

class StickS3Buttons final {
 public:
  void begin() noexcept;
  application::InputEvent poll() noexcept;
};

}  // namespace jinggua::hardware
