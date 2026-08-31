#include "test_framework.h"

#include "jinggua/hardware/microphone_research.h"

void runMicrophoneResearchTests(TestRunner& runner) {
  const jinggua::hardware::MicResearchConfig config;
  EXPECT_EQ(runner, config.sampleRateHz, 16000U);
  EXPECT_EQ(runner, config.blockSamples, static_cast<std::size_t>(256));
  EXPECT_EQ(runner, config.dmaBufferCount, static_cast<std::size_t>(4));
  EXPECT_EQ(runner, config.captureDurationMs, 3000U);

  jinggua::hardware::StickS3MicrophoneResearch research(config);
  jinggua::hardware::MicCaptureStats stats;
  // The host harness must not pretend to have a StickS3 microphone.
  EXPECT(runner, !research.begin());
  EXPECT(runner, !research.capture(stats));
}
