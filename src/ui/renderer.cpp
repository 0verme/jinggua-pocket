#include "jinggua/ui/renderer.h"

#include "jinggua/ui/history_screen.h"
#include "jinggua/ui/screens.h"

namespace jinggua::ui {

Renderer::Renderer(hardware::Display& display) noexcept : display_(display) {}

void Renderer::render(const application::StateMachine& stateMachine) noexcept {
  switch (stateMachine.state()) {
    case application::AppState::Boot:
    case application::AppState::Welcome:
      renderWelcome(display_);
      break;
    case application::AppState::Prepare:
      renderPrepare(display_);
      break;
    case application::AppState::Casting:
      renderCasting(display_, stateMachine.session());
      break;
    case application::AppState::LineResult:
      renderLineResult(display_, stateMachine.session());
      break;
    case application::AppState::HexagramResult:
      renderHexagramResult(display_, stateMachine.session());
      break;
    case application::AppState::TransformedResult:
      renderTransformedResult(display_, stateMachine.session());
      break;
    case application::AppState::ResetConfirm:
      renderResetConfirm(display_);
      break;
    case application::AppState::History:
      renderHistory(display_, stateMachine);
      break;
  }
}

}  // namespace jinggua::ui
