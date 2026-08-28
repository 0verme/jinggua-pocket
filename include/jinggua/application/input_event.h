#pragma once

namespace jinggua::application {

enum class InputEvent {
  None,
  PrimaryClick,
  SecondaryClick,
  LongPress,
  Shake,
};

const char* inputEventName(InputEvent event) noexcept;

}  // namespace jinggua::application
