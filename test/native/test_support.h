#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "jinggua/application/random_provider.h"
#include "jinggua/application/wifi_controller.h"
#include "jinggua/domain/yao.h"

class SequenceRandomProvider final : public jinggua::application::RandomProvider {
 public:
  explicit SequenceRandomProvider(std::vector<jinggua::domain::CoinSide> sequence)
      : sequence_(std::move(sequence)) {}

  jinggua::domain::CoinSide tossCoin() override {
    if (index_ >= sequence_.size()) {
      return jinggua::domain::CoinSide::Back;
    }
    return sequence_[index_++];
  }

  std::size_t consumed() const { return index_; }

 private:
  std::vector<jinggua::domain::CoinSide> sequence_;
  std::size_t index_{0};
};

inline jinggua::domain::CoinResult coinsForTotal(std::uint8_t total) {
  using jinggua::domain::CoinSide;
  if (total == 6) {
    return jinggua::domain::CoinResult::fromCoins(
        {CoinSide::Back, CoinSide::Back, CoinSide::Back});
  }
  if (total == 7) {
    return jinggua::domain::CoinResult::fromCoins(
        {CoinSide::Front, CoinSide::Back, CoinSide::Back});
  }
  if (total == 8) {
    return jinggua::domain::CoinResult::fromCoins(
        {CoinSide::Front, CoinSide::Front, CoinSide::Back});
  }
  if (total == 9) {
    return jinggua::domain::CoinResult::fromCoins(
        {CoinSide::Front, CoinSide::Front, CoinSide::Front});
  }
  assert(false && "total must be between 6 and 9");
  return {};
}

inline jinggua::domain::Yao yaoAt(std::uint8_t position,
                                  std::uint8_t total) {
  const auto yao = jinggua::domain::createYao(position, coinsForTotal(total));
  assert(yao.has_value());
  return *yao;
}

// Fake WifiController for StateMachine tests. Default state is Off.
// Call setState() to simulate connection progress or failure.
class FakeWifiController final : public jinggua::application::WifiController {
 public:
  jinggua::application::WifiState state() const noexcept override {
    return state_;
  }

  bool enable() noexcept override {
    if (state_ == jinggua::application::WifiState::Off ||
        state_ == jinggua::application::WifiState::Failed ||
        state_ == jinggua::application::WifiState::Timeout) {
      state_ = jinggua::application::WifiState::Connecting;
      ++enableCount_;
      return true;
    }
    return false;
  }

  void disable() noexcept override {
    state_ = jinggua::application::WifiState::Off;
    ++disableCount_;
  }

  jinggua::application::WifiState update(std::uint32_t /*nowMs*/) noexcept override {
    return state_;
  }

  const char* ssid() const noexcept override {
    return "TestWiFi";
  }

  bool configured() const noexcept override { return true; }

  void setState(jinggua::application::WifiState next) noexcept {
    state_ = next;
  }

  int enableCount() const noexcept { return enableCount_; }
  int disableCount() const noexcept { return disableCount_; }

 private:
  jinggua::application::WifiState state_{jinggua::application::WifiState::Off};
  int enableCount_{0};
  int disableCount_{0};
};

// Fake WifiController that refuses to connect (simulates no credentials).
class UnconfiguredFakeWifiController final
    : public jinggua::application::WifiController {
 public:
  jinggua::application::WifiState state() const noexcept override {
    return state_;
  }

  bool enable() noexcept override {
    state_ = jinggua::application::WifiState::Failed;
    return false;
  }

  void disable() noexcept override {
    state_ = jinggua::application::WifiState::Off;
  }

  jinggua::application::WifiState update(std::uint32_t) noexcept override {
    return state_;
  }

  const char* ssid() const noexcept override { return ""; }
  bool configured() const noexcept override { return false; }

 private:
  jinggua::application::WifiState state_{jinggua::application::WifiState::Off};
};
