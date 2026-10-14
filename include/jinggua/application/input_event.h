#pragma once

namespace jinggua::application {

enum class InputEvent {
  None,
  PrimaryClick,
  PrimaryLongPress,
  SecondaryClick,
  LongPress,
  Shake,
};

const char* inputEventName(InputEvent event) noexcept;

}  // namespace jinggua::application
