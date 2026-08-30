#include "jinggua/hardware/buttons.h"

#include <cstdint>

#if defined(ARDUINO)
#include <M5Unified.h>
#endif

namespace jinggua::hardware {

void StickS3Buttons::begin() noexcept {
#if defined(ARDUINO)
  // Keep the input contract explicit: M5Unified emits one click only after
  // its debounce interval and one hold event at the hold threshold.
  constexpr std::uint32_t kDebounceMs = 10;
  M5.BtnA.setDebounceThresh(kDebounceMs);
  M5.BtnB.setDebounceThresh(kDebounceMs);
#else
  // M5Unified initializes the StickS3 buttons together with the display.
#endif
}

application::InputEvent StickS3Buttons::poll() noexcept {
#if defined(ARDUINO)
  // M5Unified requires M5.update() once per loop to advance button state.
  M5.update();
  if (M5.BtnA.wasHold()) {
    Serial.println("[Input] BtnA hold -> LongPress");
    return application::InputEvent::LongPress;
  }
  if (M5.BtnB.wasHold()) {
    Serial.println("[Input] BtnB hold -> LongPress");
    return application::InputEvent::LongPress;
  }
  if (M5.BtnA.wasClicked()) {
    Serial.println("[Input] BtnA click -> PrimaryClick");
    return application::InputEvent::PrimaryClick;
  }
  if (M5.BtnB.wasClicked()) {
    Serial.println("[Input] BtnB click -> SecondaryClick");
    return application::InputEvent::SecondaryClick;
  }
#endif
  return application::InputEvent::None;
}

}  // namespace jinggua::hardware
