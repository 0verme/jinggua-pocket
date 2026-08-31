#pragma once

#include <cstdint>

#include "jinggua/application/divination_session.h"
#include "jinggua/application/input_event.h"
#include "jinggua/application/wifi_controller.h"

namespace jinggua::application {

enum class AppState {
  Boot,
  Welcome,
  Prepare,
  Casting,
  LineResult,
  HexagramResult,
  TransformedResult,
  ResetConfirm,
  Settings,
  WifiConnecting,
  WifiConnected,
  WifiFailed,
  WifiTimeout,
};

const char* appStateName(AppState state) noexcept;

class StateMachine final {
 public:
  StateMachine(DivinationSession& session, WifiController& wifi) noexcept;

  void begin() noexcept;
  void handleInput(InputEvent event) noexcept;
  void update(std::uint32_t nowMs) noexcept;

  AppState state() const noexcept { return state_; }
  const DivinationSession& session() const noexcept { return session_; }
  const WifiController& wifi() const noexcept { return wifi_; }
  bool isDirty() const noexcept { return dirty_; }
  void acknowledgeRender() noexcept { dirty_ = false; }

 private:
  void transitionTo(AppState next) noexcept;
  void castLine() noexcept;
  void handleResultAction() noexcept;

  DivinationSession& session_;
  WifiController& wifi_;
  AppState state_{AppState::Boot};
  AppState resetReturnState_{AppState::HexagramResult};
  bool dirty_{true};
};

}  // namespace jinggua::application
