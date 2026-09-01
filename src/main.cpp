#include <Arduino.h>

#include <cstdlib>
#include <limits>

#include "jinggua/application/divination_session.h"
#include "jinggua/application/ring_history_store.h"
#include "jinggua/application/state_machine.h"
#include "jinggua/hardware/audio_feedback.h"
#include "jinggua/hardware/buttons.h"
#include "jinggua/hardware/display.h"
#include "jinggua/hardware/imu.h"
#include "jinggua/hardware/microphone_research.h"
#include "jinggua/hardware/preferences_slot_storage.h"
#include "jinggua/hardware/random.h"
#include "jinggua/hardware/wifi_manager.h"
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
jinggua::hardware::Esp32WifiManager wifiManager;
jinggua::hardware::StickS3AudioController audioController;
jinggua::application::StateMachine stateMachine(session, wifiManager,
                                                 &historyStore);
jinggua::ui::Renderer renderer(display);

#if defined(JINGGUA_ENABLE_MIC_RESEARCH) && JINGGUA_ENABLE_MIC_RESEARCH
jinggua::hardware::StickS3MicrophoneResearch microphoneResearch;
char micCommandBuffer[32]{};
std::size_t micCommandLength = 0;

void printMicResearchConfig() {
  Serial.print("[MicResearch] config requested_rate_hz=");
  Serial.print(microphoneResearch.config().sampleRateHz);
  Serial.print(" duration_ms=");
  Serial.print(microphoneResearch.config().captureDurationMs);
  Serial.print(" block_samples=");
  Serial.print(microphoneResearch.config().blockSamples);
  Serial.print(" dma_buffers=");
  Serial.print(microphoneResearch.config().dmaBufferCount);
  Serial.println(" format=PCM_S16LE channels=mono");
}

void printMicStats(const char* label,
                   const jinggua::hardware::MicCaptureStats& stats) {
  Serial.print("[MicResearch] label=");
  Serial.println(label);
  Serial.print("[MicResearch] requested_rate_hz=");
  Serial.print(stats.sampleRateHz);
  Serial.print(" actual_rate_hz=");
  Serial.print(stats.effectiveSampleRateHz);
  Serial.print(" rate_stable=");
  Serial.println(stats.sampleRateStable ? "YES" : "NO");
  Serial.print("[MicResearch] duration_ms=");
  Serial.print(stats.elapsedMs);
  Serial.print(" requested_ms=");
  Serial.print(stats.requestedDurationMs);
  Serial.print(" samples=");
  Serial.print(stats.sampleCount);
  Serial.print(" expected_pcm_bytes=");
  Serial.println(stats.expectedPcmBytes);
  Serial.print("[MicResearch] encoding=PCM_S16LE channels=mono block_samples=");
  Serial.print(stats.blockSamples);
  Serial.print(" block_bytes=");
  Serial.print(stats.blockBytes);
  Serial.print(" dma_buffers=");
  Serial.print(stats.dmaBufferCount);
  Serial.print(" dma_payload_bytes_estimate=");
  Serial.print(stats.dmaPayloadBytesEstimate);
  Serial.print(" dma_read_chunk_bytes=");
  Serial.println(stats.dmaReadChunkBytes);
  Serial.print("[MicResearch] blocks=");
  Serial.print(stats.blocksCaptured);
  Serial.print(" busy_wait_ms=");
  Serial.print(stats.waitBusyMs);
  Serial.print(" max_recording_slots=");
  Serial.print(stats.maxRecordingSlots);
  Serial.print(" queue_full_observations=");
  Serial.print(stats.queueFullObservations);
  Serial.print(" record_failures=");
  Serial.print(stats.recordFailures);
  Serial.print(" record_timeouts=");
  Serial.println(stats.recordTimeouts);
  Serial.print("[MicResearch] temp_buffer_bytes=");
  Serial.print(stats.peakTemporaryBufferBytes);
  Serial.print(" internal_bytes=");
  Serial.print(stats.temporaryBufferInternalBytes);
  Serial.print(" psram_bytes=");
  Serial.println(stats.temporaryBufferPsramBytes);
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
  Serial.print(" during_min=");
  Serial.print(stats.freeHeapDuringMin);
  Serial.print(" after=");
  Serial.print(stats.freeHeapAfter);
  Serial.print(" free_psram_before=");
  Serial.print(stats.freePsramBefore);
  Serial.print(" during_min=");
  Serial.print(stats.freePsramDuringMin);
  Serial.print(" after=");
  Serial.println(stats.freePsramAfter);
}

bool parseMicResearchNumber(const char* text, std::uint32_t& value) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  char* end = nullptr;
  const auto parsed = std::strtoull(text, &end, 10);
  if (*end != '\0' || parsed == 0 ||
      parsed > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  value = static_cast<std::uint32_t>(parsed);
  return true;
}

void captureMicResearch(const char* label) {
  Serial.print("[MicResearch] capture_start label=");
  Serial.print(label);
  Serial.print(" requested_rate_hz=");
  Serial.print(microphoneResearch.config().sampleRateHz);
  Serial.print(" requested_ms=");
  Serial.println(microphoneResearch.config().captureDurationMs);

  jinggua::hardware::MicCaptureStats stats;
  const bool captured = microphoneResearch.capture(stats);
  printMicStats(label, stats);
  Serial.print("[MicResearch] result=");
  Serial.println(captured ? "OK" : "FAIL");
  Serial.println("[MicResearch] peripheral=stopped");
}

void printMicResearchHelp() {
  Serial.println(
      "[MicResearch] commands: r8000/r16000=rate Hz, d5/d15/d30=duration seconds,");
  Serial.println(
      "[MicResearch] a=ambient, v=voice, t=transient, p=print config, h=help");
}

void processMicResearchCommand(const char* command) {
  if (command[1] == '\0') {
    if (command[0] == 'a' || command[0] == 'A') {
      captureMicResearch("ambient");
    } else if (command[0] == 'v' || command[0] == 'V') {
      captureMicResearch("voice");
    } else if (command[0] == 't' || command[0] == 'T') {
      captureMicResearch("transient");
    } else if (command[0] == 'p' || command[0] == 'P') {
      printMicResearchConfig();
    } else if (command[0] == 'h' || command[0] == 'H') {
      printMicResearchHelp();
    } else {
      Serial.println("[MicResearch] unknown command; send h for help");
    }
    return;
  }

  const char prefix = command[0];
  std::uint32_t value = 0;
  if (!parseMicResearchNumber(command + 1, value)) {
    Serial.println("[MicResearch] invalid numeric command");
    return;
  }

  auto config = microphoneResearch.config();
  if (prefix == 'r' || prefix == 'R') {
    config.sampleRateHz = value;
  } else if (prefix == 'd' || prefix == 'D') {
    const auto maxSeconds =
        jinggua::hardware::kMicResearchMaxCaptureDurationMs / 1000U;
    if (value > maxSeconds) {
      Serial.println("[MicResearch] duration exceeds 60 seconds");
      return;
    }
    config.captureDurationMs = value * 1000U;
  } else {
    Serial.println("[MicResearch] unknown command; send h for help");
    return;
  }

  if (!microphoneResearch.configure(config)) {
    Serial.println("[MicResearch] config rejected");
    return;
  }
  printMicResearchConfig();
}

void pollMicResearchCommand() {
  while (Serial.available() > 0) {
    const char input = static_cast<char>(Serial.read());
    if (input == '\r' || input == '\n') {
      if (micCommandLength != 0) {
        micCommandBuffer[micCommandLength] = '\0';
        processMicResearchCommand(micCommandBuffer);
        micCommandLength = 0;
      }
      continue;
    }

    if (micCommandLength == 0 &&
        (input == 'a' || input == 'A' || input == 'v' || input == 'V' ||
         input == 't' || input == 'T' || input == 'p' || input == 'P' ||
         input == 'h' || input == 'H')) {
      char immediateCommand[2] = {input, '\0'};
      processMicResearchCommand(immediateCommand);
      continue;
    }

    if (micCommandLength + 1 >= sizeof(micCommandBuffer)) {
      micCommandLength = 0;
      Serial.println("[MicResearch] command too long");
      continue;
    }
    micCommandBuffer[micCommandLength++] = input;
  }
}
#endif

void renderIfNeeded() {
  if (!stateMachine.isDirty() && !renderer.isDirty()) {
    return;
  }
  renderer.render(stateMachine);
  stateMachine.acknowledgeRender();
  renderer.acknowledgeRender();
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

  const bool audioReady = audioController.begin();
  stateMachine.setAudioController(audioController);
  Serial.print("[Audio] speaker ");
#if defined(JINGGUA_ENABLE_MIC_RESEARCH) && JINGGUA_ENABLE_MIC_RESEARCH
  Serial.println(audioReady ? "configured but disabled in mic research"
                             : "disabled in mic research");
#else
  Serial.println(audioReady ? "configured (lazy, non-blocking playback)"
                            : "unavailable");
#endif

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
  microphoneResearch.end();
  Serial.print("[MicResearch] init ");
  Serial.println(microphoneReady ? "OK" : "FAIL");
  printMicResearchConfig();
  Serial.println(
      "[MicResearch] peripheral=stopped; no capture until an explicit command");
  printMicResearchHelp();
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
  stateMachine.update(millis());
  const auto previousState = stateMachine.state();
  const auto previousLineCount = stateMachine.session().lineCount();
  const bool animationWasActive = stateMachine.isLineAnimationActive();
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
      !animationWasActive &&
      (currentState == jinggua::application::AppState::Casting ||
       (currentState == jinggua::application::AppState::LineResult &&
        !stateMachine.session().isComplete()));
  bool shakeTriggered = false;
  if (hasSample && shakeInputActive) {
    shakeTriggered = shakeDetector.update(sample);
  } else if (!animationWasActive && !shakeInputActive) {
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
      event != jinggua::application::InputEvent::Shake &&
      !animationWasActive) {
    // A button action starts a new input gesture; do not let a partial IMU
    // candidate survive across the Button fallback path. Button input during
    // the coin animation is ignored without clearing the shake cooldown.
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

  const bool animationFinished =
      renderer.update(stateMachine, static_cast<std::uint32_t>(millis()));
  if (animationFinished) {
    stateMachine.finishLineAnimation();
  }

  renderIfNeeded();
  delay(20);
}
