#pragma once

#include <cstdint>

namespace jinggua::application {

enum class PowerState : std::uint8_t {
  Active,
  Dim,
  DisplayOff,
  LightSleep,
};

const char* powerStateName(PowerState state) noexcept;

// Power policy is kept in one value object so timeout, brightness and idle IMU
// settings cannot drift into unrelated loop or hardware code.
struct PowerPolicy final {
  static constexpr std::uint32_t kDimAfterMs = 30'000U;
  static constexpr std::uint32_t kDisplayOffAfterMs = 60'000U;
  static constexpr std::uint32_t kLightSleepAfterMs = 300'000U;
  static constexpr std::uint32_t kDisplayOffImuPollIntervalMs = 250U;
  static constexpr std::uint8_t kActiveBrightness = 128U;
  static constexpr std::uint8_t kDimBrightness = 32U;

  std::uint32_t dimAfterMs{kDimAfterMs};
  std::uint32_t displayOffAfterMs{kDisplayOffAfterMs};
  std::uint32_t lightSleepAfterMs{kLightSleepAfterMs};
  std::uint32_t displayOffImuPollIntervalMs{
      kDisplayOffImuPollIntervalMs};
  std::uint8_t activeBrightness{kActiveBrightness};
  std::uint8_t dimBrightness{kDimBrightness};
};

class PowerManager final {
 public:
  explicit PowerManager(PowerPolicy policy = PowerPolicy{}) noexcept;

  // Starts a new inactivity epoch. The initial state is always Active.
  void begin(std::uint32_t nowMs) noexcept;

  // Advances the logical state. A false sleepAllowed keeps the device Active
  // and prevents both DisplayOff and LightSleep transitions.
  bool update(std::uint32_t nowMs, bool sleepAllowed) noexcept;

  // Records a real user action. Ordinary IMU samples must not call this.
  // Returns true when the visible power state changed to Active.
  bool recordActivity(std::uint32_t nowMs) noexcept;

  // Light sleep returns without resetting the application. Wake is a user
  // activity boundary and therefore starts a fresh inactivity epoch.
  void wake(std::uint32_t nowMs) noexcept;

  // The request is separate from the logical LightSleep state so the hardware
  // adapter is called exactly once for each sleep attempt.
  bool lightSleepRequested() const noexcept {
    return lightSleepRequested_;
  }
  bool consumeLightSleepRequest() noexcept;

  PowerState state() const noexcept { return state_; }
  std::uint8_t brightness() const noexcept;
  bool imuPollingAllowed() const noexcept {
    return state_ != PowerState::LightSleep;
  }
  std::uint32_t imuPollIntervalMs() const noexcept;
  std::uint32_t lastActivityAt() const noexcept { return lastActivityAt_; }
  const PowerPolicy& policy() const noexcept { return policy_; }

 private:
  bool transitionTo(PowerState next) noexcept;

  PowerPolicy policy_{};
  PowerState state_{PowerState::Active};
  std::uint32_t lastActivityAt_{0};
  bool started_{false};
  bool lightSleepRequested_{false};
};

}  // namespace jinggua::application
