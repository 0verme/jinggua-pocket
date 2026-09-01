#include "test_framework.h"

#include "jinggua/hardware/microphone_research.h"

void runMicrophoneResearchTests(TestRunner& runner) {
  const jinggua::hardware::MicResearchConfig config;
  EXPECT_EQ(runner, config.sampleRateHz, 16000U);
  EXPECT_EQ(runner, config.blockSamples, static_cast<std::size_t>(256));
  EXPECT_EQ(runner, config.dmaBufferCount, static_cast<std::size_t>(4));
  EXPECT_EQ(runner, config.captureDurationMs, 5000U);

  jinggua::hardware::StickS3MicrophoneResearch research(config);
  EXPECT(runner, research.configure(config));

  auto eightKConfig = config;
  eightKConfig.sampleRateHz = 8000;
  eightKConfig.captureDurationMs = 15000;
  EXPECT(runner, research.configure(eightKConfig));
  EXPECT_EQ(runner, research.config().sampleRateHz, 8000U);
  EXPECT_EQ(runner, research.config().captureDurationMs, 15000U);

  auto invalidConfig = eightKConfig;
  invalidConfig.sampleRateHz = 0;
  EXPECT(runner, !research.configure(invalidConfig));
  EXPECT_EQ(runner, research.config().sampleRateHz, 8000U);
  invalidConfig = eightKConfig;
  invalidConfig.captureDurationMs =
      jinggua::hardware::kMicResearchMaxCaptureDurationMs + 1U;
  EXPECT(runner, !research.configure(invalidConfig));

  jinggua::hardware::MicCaptureStats stats;
  // The host harness must not pretend to have a StickS3 microphone.
  EXPECT(runner, !research.begin());
  EXPECT(runner, !research.capture(stats));
  EXPECT_EQ(runner, stats.requestedDurationMs, 15000U);
  EXPECT_EQ(runner, stats.sampleRateHz, 8000U);
  EXPECT_EQ(runner, stats.blockBytes, static_cast<std::size_t>(512));
  EXPECT_EQ(runner, stats.expectedPcmBytes, static_cast<std::size_t>(240000));
  EXPECT_EQ(runner, stats.dmaPayloadBytesEstimate,
            static_cast<std::size_t>(2048));
  EXPECT_EQ(runner, stats.dmaReadChunkBytes, static_cast<std::size_t>(1024));
}
