#pragma once

#include "jinggua/application/divination_session.h"
#include "jinggua/application/input_event.h"

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
};

const char* appStateName(AppState state) noexcept;

class StateMachine final {
 public:
  explicit StateMachine(DivinationSession& session) noexcept;

  void begin() noexcept;
  void handleInput(InputEvent event) noexcept;

  AppState state() const noexcept { return state_; }
  const DivinationSession& session() const noexcept { return session_; }
  bool isDirty() const noexcept { return dirty_; }
  void acknowledgeRender() noexcept { dirty_ = false; }

 private:
  void transitionTo(AppState next) noexcept;
  void handleResultAction() noexcept;

  DivinationSession& session_;
  AppState state_{AppState::Boot};
  AppState resetReturnState_{AppState::HexagramResult};
  bool dirty_{true};
};

}  // namespace jinggua::application
