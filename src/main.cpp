#include <Arduino.h>

#include "jinggua/application/divination_session.h"
#include "jinggua/application/ring_history_store.h"
#include "jinggua/application/state_machine.h"
#include "jinggua/hardware/buttons.h"
#include "jinggua/hardware/display.h"
#include "jinggua/hardware/imu.h"
#include "jinggua/hardware/microphone_research.h"
#include "jinggua/hardware/preferences_slot_storage.h"
#include "jinggua/hardware/random.h"
#include "jinggua/ui/renderer.h"

namespace {

constexpr const char* kFirmwareVersion = "hardware-v0.2";

jinggua::hardware::Display display;
jinggua::hardware::StickS3Buttons buttons;
jinggua::hardware::StickS3Imu imu;
jinggua::hardware::ShakeDetector shakeDetector;
jinggua::hardware::Esp32RandomProvider randomProvider;
jinggua::hardware::PreferencesSlotStorage slotStorage;
jinggua::application::RingHistoryStore historyStore(slotStorage);
jinggua::application::DivinationSession session(randomProvider);
jinggua::application::StateMachine stateMachine(session, &historyStore);
jinggua::ui::Renderer renderer(display);

#if defined(JINGGUA_ENABLE_MIC_RESEARCH) && JINGGUA_ENABLE_MIC_RESEARCH
jinggua::hardware::StickS3MicrophoneResearch microphoneResearch;

void printMicStats(const char* label,
                   const jinggua::hardware::MicCaptureStats& stats) {
  Serial.print("[MicResearch] label=");
  Serial.println(label);
  Serial.print("[MicResearch] duration_ms=");
  Serial.print(stats.elapsedMs);
  Serial.print(" requested_ms=");
  Serial.println(stats.requestedDurationMs);
  Serial.print("[MicResearch] sample_rate_hz=");
  Serial.print(stats.sampleRateHz);
  Serial.print(" samples=");
  Serial.print(stats.sampleCount);
  Serial.print(" effective_rate_hz=");
  if (stats.elapsedMs == 0) {
    Serial.println(0);
  } else {
    Serial.println((static_cast<std::uint64_t>(stats.sampleCount) * 1000U) /
                   stats.elapsedMs);
  }
  Serial.print("[MicResearch] encoding=PCM_S16LE channels=mono block_samples=");
  Serial.print(stats.blockSamples);
  Serial.print(" block_bytes=");
  Serial.print(stats.blockBytes);
  Serial.print(" dma_buffers=");
  Serial.println(stats.dmaBufferCount);
  Serial.print("[MicResearch] min=");
  Serial.print(stats.minimum);
  Serial.print(" max=");
  Serial.print(stats.maximum);
  Serial.print(" peak=");
  Serial.print(stats.peak);
  Serial.print(" mean_milli=");
  Serial.print(stats.meanMilli);
  Serial.print(" rms_milli=");
  Serial.print(stats.rmsMilli);
  Serial.print(" clipped=");
  Serial.print(stats.clippedSamples);
  Serial.print(" zero_crossings=");
  Serial.println(stats.zeroCrossings);
  Serial.print("[MicResearch] free_heap_before=");
  Serial.print(stats.freeHeapBefore);
  Serial.print(" after=");
  Serial.print(stats.freeHeapAfter);
  Serial.print(" free_psram_before=");
  Serial.print(stats.freePsramBefore);
  Serial.print(" after=");
  Serial.println(stats.freePsramAfter);
}

void pollMicResearchCommand() {
  while (Serial.available() > 0) {
    const char command = static_cast<char>(Serial.read());
    const char* label = nullptr;
    if (command == 'a' || command == 'A') {
      label = "ambient";
    } else if (command == 'v' || command == 'V') {
      label = "voice";
    } else if (command == 't' || command == 'T') {
      label = "transient";
    } else {
      continue;
    }

    Serial.print("[MicResearch] capture_start label=");
    Serial.println(label);
    jinggua::hardware::MicCaptureStats stats;
    if (microphoneResearch.capture(stats)) {
      printMicStats(label, stats);
    } else {
      Serial.println("[MicResearch] capture FAIL");
    }
  }
}
#endif

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

  const bool historyReady = historyStore.begin();
  Serial.print("[History] init ");
  Serial.println(historyReady ? "OK" : "FAIL");
  Serial.print("[History] records=");
  Serial.println(static_cast<unsigned long>(historyStore.count()));
  Serial.print("[History] capacity=");
  Serial.println(static_cast<unsigned long>(historyStore.capacity()));

#if defined(JINGGUA_ENABLE_MIC_RESEARCH) && JINGGUA_ENABLE_MIC_RESEARCH
  const bool microphoneReady = microphoneResearch.begin();
  Serial.print("[MicResearch] init ");
  Serial.println(microphoneReady ? "OK" : "FAIL");
  Serial.print("[MicResearch] sample_rate_hz=");
  Serial.print(microphoneResearch.config().sampleRateHz);
  Serial.print(" block_samples=");
  Serial.print(microphoneResearch.config().blockSamples);
  Serial.print(" dma_buffers=");
  Serial.print(microphoneResearch.config().dmaBufferCount);
  Serial.println(" encoding=PCM_S16LE mono");
  Serial.println(
      "[MicResearch] commands: a=ambient, v=voice, t=transient; each captures 3s");
#endif

  stateMachine.begin();
  Serial.print("[State] BOOT -> ");
  Serial.println(jinggua::application::appStateName(stateMachine.state()));
  renderIfNeeded();
}

void loop() {
#if defined(JINGGUA_ENABLE_MIC_RESEARCH) && JINGGUA_ENABLE_MIC_RESEARCH
  pollMicResearchCommand();
#endif
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

  const auto currentState = stateMachine.state();
  const bool shakeInputActive =
      currentState == jinggua::application::AppState::Casting ||
      (currentState == jinggua::application::AppState::LineResult &&
       !stateMachine.session().isComplete());
  bool shakeTriggered = false;
  if (hasSample && shakeInputActive) {
    shakeTriggered = shakeDetector.update(sample);
  } else if (!shakeInputActive) {
    shakeDetector.reset();
  }
  if (event == jinggua::application::InputEvent::None && shakeTriggered) {
    Serial.println("[Shake] TRIGGERED");
    event = jinggua::application::InputEvent::Shake;
  }

  if (event != jinggua::application::InputEvent::None) {
    Serial.print("[Input] ");
    Serial.println(jinggua::application::inputEventName(event));
  }

  stateMachine.handleInput(event);
  if (event != jinggua::application::InputEvent::None &&
      event != jinggua::application::InputEvent::Shake) {
    // A button action starts a new input gesture; do not let a partial IMU
    // candidate survive across the Button fallback path.
    shakeDetector.reset();
  }
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
