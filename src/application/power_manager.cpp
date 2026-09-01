#include "jinggua/application/power_manager.h"

#include <cstdint>

namespace jinggua::application {
namespace {

std::uint32_t elapsedSince(std::uint32_t nowMs,
                           std::uint32_t thenMs) noexcept {
  return nowMs - thenMs;
}

}  // namespace

const char* powerStateName(PowerState state) noexcept {
  switch (state) {
    case PowerState::Active:
      return "Active";
    case PowerState::Dim:
      return "Dim";
    case PowerState::DisplayOff:
      return "DisplayOff";
    case PowerState::LightSleep:
      return "LightSleep";
  }
  return "Unknown";
}

PowerManager::PowerManager(PowerPolicy policy) noexcept : policy_(policy) {}

void PowerManager::begin(std::uint32_t nowMs) noexcept {
  lastActivityAt_ = nowMs;
  state_ = PowerState::Active;
  started_ = true;
  lightSleepRequested_ = false;
}

bool PowerManager::transitionTo(PowerState next) noexcept {
  if (state_ == next) {
    return false;
  }
  state_ = next;
  lightSleepRequested_ = next == PowerState::LightSleep;
  return true;
}

bool PowerManager::recordActivity(std::uint32_t nowMs) noexcept {
  if (!started_) {
    begin(nowMs);
    return false;
  }
  lastActivityAt_ = nowMs;
  lightSleepRequested_ = false;
  return transitionTo(PowerState::Active);
}

void PowerManager::wake(std::uint32_t nowMs) noexcept {
  recordActivity(nowMs);
}

bool PowerManager::update(std::uint32_t nowMs, bool sleepAllowed) noexcept {
  if (!started_) {
    begin(nowMs);
    return false;
  }

  if (!sleepAllowed) {
    lightSleepRequested_ = false;
    return transitionTo(PowerState::Active);
  }

  if (state_ == PowerState::LightSleep) {
    return false;
  }

  const auto idleMs = elapsedSince(nowMs, lastActivityAt_);
  PowerState next = PowerState::Active;
  if (idleMs >= policy_.lightSleepAfterMs) {
    next = PowerState::LightSleep;
  } else if (idleMs >= policy_.displayOffAfterMs) {
    next = PowerState::DisplayOff;
  } else if (idleMs >= policy_.dimAfterMs) {
    next = PowerState::Dim;
  }
  return transitionTo(next);
}

bool PowerManager::consumeLightSleepRequest() noexcept {
  if (!lightSleepRequested_) {
    return false;
  }
  lightSleepRequested_ = false;
  return true;
}

std::uint8_t PowerManager::brightness() const noexcept {
  switch (state_) {
    case PowerState::Dim:
      return policy_.dimBrightness;
    case PowerState::Active:
      return policy_.activeBrightness;
    case PowerState::DisplayOff:
    case PowerState::LightSleep:
      return 0U;
  }
  return 0U;
}

std::uint32_t PowerManager::imuPollIntervalMs() const noexcept {
  return state_ == PowerState::DisplayOff
             ? policy_.displayOffImuPollIntervalMs
             : 0U;
}

}  // namespace jinggua::application
