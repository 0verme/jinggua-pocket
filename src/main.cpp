#include <Arduino.h>

#include "jinggua/application/divination_session.h"
#include "jinggua/application/state_machine.h"
#include "jinggua/hardware/buttons.h"
#include "jinggua/hardware/display.h"
#include "jinggua/hardware/imu.h"
#include "jinggua/hardware/random.h"
#include "jinggua/ui/renderer.h"

namespace {

constexpr const char* kFirmwareVersion = "hardware-v0.1";

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
  Serial.begin(115200);
  Serial.println("[JingGua] boot");
  Serial.print("[JingGua] version=");
  Serial.println(kFirmwareVersion);

  Serial.println("[Hardware] StickS3 init");
  const bool displayReady = display.begin();
  Serial.print("[Display] init ");
  Serial.println(displayReady ? "OK" : "FAIL");
  Serial.print("[Display] size=");
  Serial.print(display.width());
  Serial.print("x");
  Serial.println(display.height());

  buttons.begin();
  Serial.println("[Button] init OK");
  imu.begin();
  Serial.println("[IMU] init OK");

  stateMachine.begin();
  Serial.print("[State] BOOT -> ");
  Serial.println(jinggua::application::appStateName(stateMachine.state()));
  renderIfNeeded();
}

void loop() {
  const auto previousState = stateMachine.state();
  const auto previousLineCount = stateMachine.session().lineCount();
  auto event = buttons.poll();

  jinggua::hardware::ImuSample sample;
  const bool hasSample = imu.read(sample);
  static std::uint32_t lastImuLogAt = 0;
  if (hasSample && sample.timestampMs - lastImuLogAt >= 250) {
    lastImuLogAt = sample.timestampMs;
    Serial.print("[IMU] a=(");
    Serial.print(sample.accelerationX, 2);
    Serial.print(",");
    Serial.print(sample.accelerationY, 2);
    Serial.print(",");
    Serial.print(sample.accelerationZ, 2);
    Serial.print(") g=(");
    Serial.print(sample.angularVelocityX, 2);
    Serial.print(",");
    Serial.print(sample.angularVelocityY, 2);
    Serial.print(",");
    Serial.print(sample.angularVelocityZ, 2);
    Serial.println(")");
  }

  if (event == jinggua::application::InputEvent::None && hasSample &&
      shakeDetector.update(sample)) {
    Serial.println("[Shake] TRIGGERED");
    event = jinggua::application::InputEvent::Shake;
  }

  if (event != jinggua::application::InputEvent::None) {
    Serial.print("[Input] ");
    Serial.println(jinggua::application::inputEventName(event));
  }

  stateMachine.handleInput(event);
  if (stateMachine.state() != previousState) {
    Serial.print("[State] ");
    Serial.print(jinggua::application::appStateName(previousState));
    Serial.print(" -> ");
    Serial.println(jinggua::application::appStateName(stateMachine.state()));
  }
  if (stateMachine.session().lineCount() > previousLineCount) {
    const auto* line = stateMachine.session().latestLine();
    if (line != nullptr) {
      Serial.print("[Cast] line=");
      Serial.println(line->position);
      Serial.print("[Coin] ");
      Serial.print(static_cast<unsigned>(line->coins.coins[0]));
      Serial.print(",");
      Serial.print(static_cast<unsigned>(line->coins.coins[1]));
      Serial.print(",");
      Serial.print(static_cast<unsigned>(line->coins.coins[2]));
      Serial.print(" total=");
      Serial.println(line->coins.total);
      Serial.print("[Yao] ");
      Serial.println(jinggua::domain::yaoTypeName(line->type));
    }
  }

  renderIfNeeded();
  delay(20);
}
