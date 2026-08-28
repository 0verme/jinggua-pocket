#include "test_framework.h"

#include "jinggua/hardware/imu.h"

namespace {

jinggua::hardware::ImuSample sampleAt(float acceleration,
                                       std::uint32_t timestampMs) {
  jinggua::hardware::ImuSample sample;
  sample.accelerationX = acceleration;
  sample.timestampMs = timestampMs;
  return sample;
}

}  // namespace

void runImuTests(TestRunner& runner) {
  using jinggua::hardware::ShakeDetector;
  using jinggua::hardware::ShakeDetectorConfig;

  const ShakeDetectorConfig config{1.5F, 500, 1000, 2};
  ShakeDetector detector(config);
  EXPECT(runner, !detector.update(sampleAt(2.0F, 0)));
  EXPECT(runner, !detector.update(sampleAt(0.0F, 100)));
  EXPECT(runner, detector.update(sampleAt(2.0F, 200)));

  // A second threshold crossing during cooldown cannot create another SHAKE.
  EXPECT(runner, !detector.update(sampleAt(0.0F, 300)));
  EXPECT(runner, !detector.update(sampleAt(2.0F, 400)));

  // The next gesture needs two crossings inside its own detection window.
  EXPECT(runner, !detector.update(sampleAt(0.0F, 1300)));
  EXPECT(runner, !detector.update(sampleAt(2.0F, 1400)));
  EXPECT(runner, !detector.update(sampleAt(0.0F, 1450)));
  EXPECT(runner, detector.update(sampleAt(2.0F, 1500)));

  ShakeDetector windowDetector(config);
  EXPECT(runner, !windowDetector.update(sampleAt(2.0F, 0)));
  EXPECT(runner, !windowDetector.update(sampleAt(0.0F, 100)));
  EXPECT(runner, !windowDetector.update(sampleAt(2.0F, 600)));
  EXPECT(runner, !windowDetector.update(sampleAt(0.0F, 700)));
  EXPECT(runner, windowDetector.update(sampleAt(2.0F, 800)));
}
