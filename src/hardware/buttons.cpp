#include "jinggua/hardware/buttons.h"

#if defined(ARDUINO)
#include <M5Unified.h>
#endif

namespace jinggua::hardware {

void StickS3Buttons::begin() noexcept {
  // M5Unified initializes the StickS3 buttons together with the display.
}

application::InputEvent StickS3Buttons::poll() noexcept {
#if defined(ARDUINO)
  // M5Unified requires M5.update() once per loop to advance button state.
  M5.update();
  if (M5.BtnA.wasHold() || M5.BtnB.wasHold()) {
    return application::InputEvent::LongPress;
  }
  if (M5.BtnA.wasClicked()) {
    return application::InputEvent::PrimaryClick;
  }
  if (M5.BtnB.wasClicked()) {
    return application::InputEvent::SecondaryClick;
  }
#endif
  return application::InputEvent::None;
}

}  // namespace jinggua::hardware
