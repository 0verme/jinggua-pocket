#include "jinggua/application/input_event.h"

namespace jinggua::application {

const char* inputEventName(InputEvent event) noexcept {
  switch (event) {
    case InputEvent::None:
      return "NONE";
    case InputEvent::PrimaryClick:
      return "PRIMARY_CLICK";
    case InputEvent::SecondaryClick:
      return "SECONDARY_CLICK";
    case InputEvent::LongPress:
      return "LONG_PRESS";
    case InputEvent::Shake:
      return "SHAKE";
  }
  return "UNKNOWN";
}

}  // namespace jinggua::application
