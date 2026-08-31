#pragma once

#include "jinggua/application/divination_session.h"
#include "jinggua/application/history_store.h"
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
  History,
};

const char* appStateName(AppState state) noexcept;

class StateMachine final {
 public:
  // Construct a state machine. `history` is optional — when nullptr, history
  // persistence is silently skipped and the History state shows an empty list.
  // Existing tests construct StateMachine(session) without the second
  // argument and continue to work unchanged.
  explicit StateMachine(DivinationSession& session,
                        HistoryStore* history = nullptr) noexcept;

  void begin() noexcept;
  void handleInput(InputEvent event) noexcept;

  AppState state() const noexcept { return state_; }
  const DivinationSession& session() const noexcept { return session_; }
  bool isDirty() const noexcept { return dirty_; }
  void acknowledgeRender() noexcept { dirty_ = false; }

  // ── History ────────────────────────────────────────────────────────
  const HistoryStore* history() const noexcept { return history_; }
  std::size_t historyCursor() const noexcept { return historyCursor_; }

 private:
  void transitionTo(AppState next) noexcept;
  void castLine() noexcept;
  void handleResultAction() noexcept;

  // Move the history browser cursor by a signed offset, clamped to the
  // available record range.
  void moveHistoryCursor(int delta) noexcept;

  // After a complete divination, persist the result to the history store.
  void persistIfComplete() noexcept;

  DivinationSession& session_;
  HistoryStore* history_;
  AppState state_{AppState::Boot};
  AppState resetReturnState_{AppState::HexagramResult};
  std::size_t historyCursor_{0};
  bool dirty_{true};
};

}  // namespace jinggua::application