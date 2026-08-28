#include <Arduino.h>

#include "jinggua/application/divination_session.h"
#include "jinggua/application/state_machine.h"
#include "jinggua/hardware/buttons.h"
#include "jinggua/hardware/display.h"
#include "jinggua/hardware/imu.h"
#include "jinggua/hardware/random.h"
#include "jinggua/ui/renderer.h"

namespace {

jinggua::hardware::Display display;
jinggua::hardware::StickS3Buttons buttons;
jinggua::hardware::StickS3Imu imu;
jinggua::hardware::ShakeDetector shakeDetector;
jinggua::hardware::Esp32RandomProvider randomProvider;
jinggua::application::DivinationSession session(randomProvider);
jinggua::application::StateMachine stateMachine(session);
jinggua::ui::Renderer renderer(display);

void renderIfNeeded() {
  if (!stateMachine.isDirty()) {
    return;
  }
  renderer.render(stateMachine);
  stateMachine.acknowledgeRender();
}

}  // namespace

void setup() {
  display.begin();
  buttons.begin();
  imu.begin();
  stateMachine.begin();
  renderIfNeeded();
}

void loop() {
  auto event = buttons.poll();

  jinggua::hardware::ImuSample sample;
  if (event == jinggua::application::InputEvent::None && imu.read(sample) &&
      shakeDetector.update(sample)) {
    event = jinggua::application::InputEvent::Shake;
  }

  stateMachine.handleInput(event);
  renderIfNeeded();
  delay(20);
}
