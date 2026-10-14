#include "jinggua/application/state_machine.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "jinggua/domain/history_record.h"

namespace jinggua::application {
namespace {

bool isLongPress(InputEvent event) noexcept {
  return event == InputEvent::LongPress ||
         event == InputEvent::PrimaryLongPress;
}

bool deadlineReached(std::uint32_t nowMs,
                     std::uint32_t deadlineMs) noexcept {
  return static_cast<std::uint32_t>(nowMs - deadlineMs) < 0x80000000U;
}

}  // namespace

const char* appStateName(AppState state) noexcept {
  switch (state) {
    case AppState::Boot:
      return "BOOT";
    case AppState::Welcome:
      return "WELCOME";
    case AppState::Prepare:
      return "PREPARE";
    case AppState::Casting:
      return "CASTING";
    case AppState::LineResult:
      return "LINE_RESULT";
    case AppState::HexagramResult:
      return "HEXAGRAM_RESULT";
    case AppState::TransformedResult:
      return "TRANSFORMED_RESULT";
    case AppState::ResetConfirm:
      return "RESET_CONFIRM";
    case AppState::History:
      return "HISTORY";
    case AppState::Settings:
      return "SETTINGS";
    case AppState::WifiConnecting:
      return "WIFI_CONNECTING";
    case AppState::WifiConnected:
      return "WIFI_CONNECTED";
    case AppState::WifiFailed:
      return "WIFI_FAILED";
    case AppState::WifiTimeout:
      return "WIFI_TIMEOUT";
    case AppState::Uploading:
      return "UPLOADING";
    case AppState::UploadSuccess:
      return "UPLOAD_SUCCESS";
    case AppState::UploadFailed:
      return "UPLOAD_FAILED";
    case AppState::OtaPrompt:
      return "OTA_PROMPT";
    case AppState::OtaChecking:
      return "OTA_CHECKING";
    case AppState::OtaUpToDate:
      return "OTA_UP_TO_DATE";
    case AppState::OtaAvailable:
      return "OTA_AVAILABLE";
    case AppState::OtaDownloading:
      return "OTA_DOWNLOADING";
    case AppState::OtaFinalizing:
      return "OTA_FINALIZING";
    case AppState::OtaCancelling:
      return "OTA_CANCELLING";
    case AppState::OtaCancelled:
      return "OTA_CANCELLED";
    case AppState::OtaRebooting:
      return "OTA_REBOOTING";
    case AppState::OtaFailed:
      return "OTA_FAILED";
  }
  return "UNKNOWN";
}

StateMachine::StateMachine(DivinationSession& session,
                           HistoryStore* history) noexcept
    : session_(session), history_(history) {}

StateMachine::StateMachine(DivinationSession& session, WifiController& wifi,
                           HistoryStore* history) noexcept
    : session_(session), history_(history), wifi_(&wifi) {}

StateMachine::StateMachine(DivinationSession& session, WifiController& wifi,
                           ApiClient& apiClient, HistoryStore* history) noexcept
    : session_(session),
      history_(history),
      wifi_(&wifi),
      apiClient_(&apiClient) {}

void StateMachine::setAudioController(AudioController& audio) noexcept {
  audio_ = &audio;
  audio_->setEnabled(soundEnabled_);
}

void StateMachine::setFirmwareUpdateClient(
    FirmwareUpdateClient& client) noexcept {
  firmwareUpdateClient_ = &client;
}

void StateMachine::setSoundEnabled(bool enabled) noexcept {
  if (soundEnabled_ == enabled) {
    return;
  }
  soundEnabled_ = enabled;
  if (audio_ != nullptr) {
    audio_->setEnabled(enabled);
  }
  dirty_ = true;
}

void StateMachine::begin() noexcept {
  if (state_ == AppState::Boot) {
    transitionTo(AppState::Welcome);
  }
}

void StateMachine::handleInput(InputEvent event) noexcept {
  if (event == InputEvent::None || autoCompleting_) {
    return;
  }
  if (lineAnimationActive_) {
    if (event == InputEvent::PrimaryLongPress &&
        (state_ == AppState::Casting || state_ == AppState::LineResult) &&
        !session_.isComplete()) {
      // Latch the one permitted input while an existing coin animation is
      // running; the next line is not generated until that animation settles.
      startAutoCompletion();
    }
    return;
  }

  switch (state_) {
    case AppState::Boot:
      begin();
      break;
    case AppState::Welcome:
      if (event == InputEvent::PrimaryClick) {
        transitionTo(AppState::Prepare);
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(wifi_ != nullptr ? AppState::Settings : AppState::History);
      } else if (isLongPress(event)) {
        // Keep the Wi-Fi shortcut on B while exposing history on the long
        // press path when both optional features are available.
        transitionTo(AppState::History);
      }
      break;
    case AppState::Prepare:
      if (event == InputEvent::PrimaryClick) {
        session_.reset();
        localRecordId_ = 0;
        uploadStarted_ = false;
        transitionTo(AppState::Casting);
        playSound(SoundCue::Start);
      }
      break;
    case AppState::Casting:
      if (event == InputEvent::PrimaryLongPress) {
        startAutoCompletion();
      } else if (event == InputEvent::PrimaryClick ||
                 event == InputEvent::Shake) {
        castLine();
      }
      break;
    case AppState::LineResult:
      if (event == InputEvent::PrimaryLongPress) {
        startAutoCompletion();
      } else if (event == InputEvent::Shake && !session_.isComplete()) {
        // Shake is an acknowledgement and the next cast in one gesture, so
        // six shakes can complete a session without requiring button input.
        castLine();
      } else if (event == InputEvent::PrimaryClick) {
        transitionTo(session_.isComplete() ? AppState::HexagramResult
                                            : AppState::Casting);
      }
      break;
    case AppState::HexagramResult:
      if (event == InputEvent::PrimaryClick) {
        handleResultAction();
      } else if (event == InputEvent::SecondaryClick) {
        uploadReturnState_ = AppState::HexagramResult;
        requestUpload();
      }
      break;
    case AppState::TransformedResult:
      if (event == InputEvent::PrimaryClick) {
        resetReturnState_ = AppState::TransformedResult;
        transitionTo(AppState::ResetConfirm);
      } else if (event == InputEvent::SecondaryClick) {
        uploadReturnState_ = AppState::TransformedResult;
        requestUpload();
      }
      break;
    case AppState::ResetConfirm:
      if (event == InputEvent::PrimaryClick || isLongPress(event)) {
        session_.reset();
        localRecordId_ = 0;
        uploadStarted_ = false;
        transitionTo(AppState::Prepare);
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(resetReturnState_);
      }
      break;
    case AppState::History:
      if (event == InputEvent::PrimaryClick) {
        moveHistoryCursor(-1);  // A = older
      } else if (event == InputEvent::SecondaryClick) {
        moveHistoryCursor(1);  // B = newer
      } else if (isLongPress(event)) {
        transitionTo(AppState::Welcome);
      }
      break;
    case AppState::Settings:
      if (event == InputEvent::PrimaryClick) {
        // Only the user can trigger a connection. If no credentials are
        // configured the controller refuses and we show a failure screen.
        if (wifi_->state() == WifiState::Connected) {
          transitionTo(AppState::WifiConnected);
        } else if (wifi_->enable()) {
          transitionTo(AppState::WifiConnecting);
        } else {
          transitionTo(AppState::WifiFailed);
        }
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Welcome);
      } else if (isLongPress(event)) {
        // Keep the existing Wi-Fi settings flow on A/B and use the existing
        // long-press gesture for the small runtime-only sound toggle.
        toggleSoundEnabled();
      }
      break;
    case AppState::WifiConnecting:
      if (event == InputEvent::SecondaryClick) {
        wifi_->disable();
        transitionTo(AppState::Settings);
      }
      break;
    case AppState::WifiConnected:
      if (event == InputEvent::PrimaryClick) {
        wifi_->disable();
        transitionTo(AppState::Settings);
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Settings);
      } else if (isLongPress(event)) {
        transitionTo(AppState::OtaPrompt);
      }
      break;
    case AppState::WifiFailed:
      if (event == InputEvent::PrimaryClick) {
        if (wifi_->enable()) {
          transitionTo(AppState::WifiConnecting);
        }
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Settings);
      }
      break;
    case AppState::WifiTimeout:
      if (event == InputEvent::PrimaryClick) {
        if (wifi_->enable()) {
          transitionTo(AppState::WifiConnecting);
        }
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Settings);
      }
      break;
    case AppState::Uploading:
      // Upload is advanced by update(); input cannot create a second request.
      break;
    case AppState::UploadSuccess:
      if (event == InputEvent::PrimaryClick ||
          event == InputEvent::SecondaryClick) {
        transitionTo(uploadReturnState_);
      }
      break;
    case AppState::UploadFailed:
      if (event == InputEvent::PrimaryClick) {
        // Retry is always another explicit user action; there is no timer or
        // background retry path.
        requestUpload();
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(uploadReturnState_);
      }
      break;
    case AppState::OtaPrompt:
      if (event == InputEvent::PrimaryClick) {
        requestFirmwareCheck();
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Welcome);
      }
      break;
    case AppState::OtaChecking:
    case AppState::OtaDownloading:
      if (event == InputEvent::SecondaryClick) {
        cancelFirmwareOperation();
      }
      break;
    case AppState::OtaAvailable:
      if (event == InputEvent::PrimaryClick) {
        requestFirmwareInstall();
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Welcome);
      }
      break;
    case AppState::OtaUpToDate:
    case AppState::OtaCancelled:
      if (event == InputEvent::PrimaryClick) {
        transitionTo(AppState::OtaPrompt);
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Welcome);
      }
      break;
    case AppState::OtaFailed:
      if (event == InputEvent::PrimaryClick) {
        requestFirmwareCheck();
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Welcome);
      }
      break;
    case AppState::OtaCancelling:
    case AppState::OtaFinalizing:
    case AppState::OtaRebooting:
      break;
  }
}

bool StateMachine::sleepAllowed() const noexcept {
  if (lineAnimationActive_ || autoCompleting_) {
    return false;
  }

  switch (state_) {
    case AppState::ResetConfirm:
    case AppState::WifiConnecting:
    case AppState::WifiConnected:
    case AppState::OtaChecking:
    case AppState::OtaDownloading:
    case AppState::OtaCancelling:
    case AppState::OtaFinalizing:
    case AppState::OtaRebooting:
      return false;
    default:
      return true;
  }
}

void StateMachine::update(std::uint32_t nowMs) noexcept {
  if (autoCompleting_) {
    updateAutoCompletion(nowMs);
    return;
  }

  if (state_ == AppState::OtaChecking ||
      state_ == AppState::OtaDownloading ||
      state_ == AppState::OtaCancelling ||
      state_ == AppState::OtaFinalizing) {
    updateFirmwareOperation();
    return;
  }

  if (state_ == AppState::Uploading) {
    if (!uploadStarted_) {
      if (wifi_ == nullptr) {
        lastApiResult_ = ApiResult::failure(ApiError::WifiUnavailable);
        transitionTo(AppState::UploadFailed);
      } else if (wifi_->state() != WifiState::Connected) {
        lastApiResult_ = ApiResult::failure(ApiError::Offline);
        transitionTo(AppState::UploadFailed);
      } else if (apiClient_ == nullptr) {
        lastApiResult_ = ApiResult::failure(ApiError::TransportError);
        transitionTo(AppState::UploadFailed);
      } else if (!apiClient_->beginUpload(session_, localRecordId_)) {
        lastApiResult_ = ApiResult::failure(ApiError::TransportError);
        transitionTo(AppState::UploadFailed);
      } else {
        uploadStarted_ = true;
      }
      return;
    }

    if (apiClient_ == nullptr) {
      lastApiResult_ = ApiResult::failure(ApiError::TransportError);
      uploadStarted_ = false;
      transitionTo(AppState::UploadFailed);
      return;
    }
    lastApiResult_ = apiClient_->poll();
    if (!lastApiResult_.isPending()) {
      uploadStarted_ = false;
      transitionTo(lastApiResult_.isSuccess() ? AppState::UploadSuccess
                                              : AppState::UploadFailed);
    }
    return;
  }

  if (wifi_ == nullptr || state_ != AppState::WifiConnecting) {
    return;
  }
  switch (wifi_->update(nowMs)) {
    case WifiState::Connected:
      transitionTo(AppState::WifiConnected);
      break;
    case WifiState::Failed:
      transitionTo(AppState::WifiFailed);
      break;
    case WifiState::Timeout:
      transitionTo(AppState::WifiTimeout);
      break;
    default:
      break;
  }
}

void StateMachine::finishLineAnimation() noexcept {
  if (!lineAnimationActive_) {
    return;
  }
  lineAnimationActive_ = false;
  dirty_ = true;
}

void StateMachine::startAutoCompletion() noexcept {
  if (autoCompleting_ || session_.isComplete() ||
      (state_ != AppState::Casting && state_ != AppState::LineResult)) {
    return;
  }

  autoCompleting_ = true;
  autoResultWaitStarted_ = false;
  dirty_ = true;
  if (!lineAnimationActive_) {
    castLine();
  }
}

void StateMachine::updateAutoCompletion(std::uint32_t nowMs) noexcept {
  if (lineAnimationActive_) {
    return;
  }

  if (session_.isComplete()) {
    autoCompleting_ = false;
    autoResultWaitStarted_ = false;
    transitionTo(AppState::HexagramResult);
    return;
  }

  if (!autoResultWaitStarted_) {
    autoAdvanceAtMs_ = nowMs + kAutoLineResultDisplayMs;
    autoResultWaitStarted_ = true;
    return;
  }
  if (!deadlineReached(nowMs, autoAdvanceAtMs_)) {
    return;
  }

  autoResultWaitStarted_ = false;
  castLine();
}

void StateMachine::castLine() noexcept {
  if (session_.castLine()) {
    lineAnimationActive_ = true;
    // The final line uses Complete instead of Cast, so one gesture produces
    // one cue and never plays both confirmations at the same time.
    playSound(session_.isComplete() ? SoundCue::Complete : SoundCue::Cast);
    persistIfComplete();
    transitionTo(AppState::LineResult);
    // The state does not change when Shake advances from one result to the
    // next, but the renderer still needs to show the new line.
    dirty_ = true;
  }
}

void StateMachine::playSound(SoundCue cue) noexcept {
  if (soundEnabled_ && audio_ != nullptr) {
    audio_->play(cue);
  }
}

void StateMachine::persistIfComplete() noexcept {
  localRecordId_ = 0;
  if (history_ == nullptr || !session_.isComplete()) {
    return;
  }
  const auto& result = session_.result();
  if (!result.has_value()) {
    return;
  }
  auto record = domain::makeHistoryRecord(*result);
  if (record.has_value() && history_->add(*record)) {
    localRecordId_ = record->localRecordId;
  }
}

void StateMachine::requestFirmwareCheck() noexcept {
  if (wifi_ == nullptr || wifi_->state() != WifiState::Connected) {
    lastFirmwareUpdateStatus_ = FirmwareUpdateStatus{};
    lastFirmwareUpdateStatus_.state = FirmwareUpdateState::Failed;
    lastFirmwareUpdateStatus_.error = FirmwareUpdateError::Offline;
    transitionTo(AppState::OtaFailed);
    return;
  }
  if (firmwareUpdateClient_ == nullptr) {
    lastFirmwareUpdateStatus_ = FirmwareUpdateStatus{};
    lastFirmwareUpdateStatus_.state = FirmwareUpdateState::Failed;
    lastFirmwareUpdateStatus_.error = FirmwareUpdateError::NotConfigured;
    transitionTo(AppState::OtaFailed);
    return;
  }
  if (!firmwareUpdateClient_->beginCheck()) {
    lastFirmwareUpdateStatus_ = firmwareUpdateClient_->status();
    if (lastFirmwareUpdateStatus_.state != FirmwareUpdateState::Failed) {
      lastFirmwareUpdateStatus_.state = FirmwareUpdateState::Failed;
      lastFirmwareUpdateStatus_.error =
          FirmwareUpdateError::ResourceUnavailable;
    }
    transitionTo(AppState::OtaFailed);
    return;
  }
  lastFirmwareUpdateStatus_ = firmwareUpdateClient_->status();
  transitionTo(AppState::OtaChecking);
}

void StateMachine::requestFirmwareInstall() noexcept {
  if (firmwareUpdateClient_ == nullptr) {
    lastFirmwareUpdateStatus_ = FirmwareUpdateStatus{};
    lastFirmwareUpdateStatus_.state = FirmwareUpdateState::Failed;
    lastFirmwareUpdateStatus_.error = FirmwareUpdateError::NotConfigured;
    transitionTo(AppState::OtaFailed);
    return;
  }
  if (!firmwareUpdateClient_->beginInstall()) {
    lastFirmwareUpdateStatus_ = firmwareUpdateClient_->status();
    if (lastFirmwareUpdateStatus_.state == FirmwareUpdateState::Available) {
      // The manifest task may still be releasing its one-shot worker after it
      // published the result. Stay on the confirmation screen so a later
      // explicit press can start the install without a false failure.
      transitionTo(AppState::OtaAvailable);
      return;
    }
    if (lastFirmwareUpdateStatus_.state != FirmwareUpdateState::Failed) {
      lastFirmwareUpdateStatus_.state = FirmwareUpdateState::Failed;
      lastFirmwareUpdateStatus_.error =
          FirmwareUpdateError::ResourceUnavailable;
    }
    transitionTo(AppState::OtaFailed);
    return;
  }
  lastFirmwareUpdateStatus_ = firmwareUpdateClient_->status();
  transitionTo(AppState::OtaDownloading);
}

void StateMachine::cancelFirmwareOperation() noexcept {
  if (firmwareUpdateClient_ == nullptr) {
    return;
  }
  firmwareUpdateClient_->cancel();
  lastFirmwareUpdateStatus_ = firmwareUpdateClient_->status();
  switch (lastFirmwareUpdateStatus_.state) {
    case FirmwareUpdateState::Finalizing:
      transitionTo(AppState::OtaFinalizing);
      break;
    case FirmwareUpdateState::Rebooting:
      transitionTo(AppState::OtaRebooting);
      break;
    case FirmwareUpdateState::Failed:
      transitionTo(AppState::OtaFailed);
      break;
    default:
      transitionTo(AppState::OtaCancelling);
      break;
  }
}

void StateMachine::updateFirmwareOperation() noexcept {
  if (firmwareUpdateClient_ == nullptr) {
    lastFirmwareUpdateStatus_ = FirmwareUpdateStatus{};
    lastFirmwareUpdateStatus_.state = FirmwareUpdateState::Failed;
    lastFirmwareUpdateStatus_.error = FirmwareUpdateError::NotConfigured;
    transitionTo(AppState::OtaFailed);
    return;
  }

  const auto previousStatus = lastFirmwareUpdateStatus_;
  lastFirmwareUpdateStatus_ = firmwareUpdateClient_->status();
  if (previousStatus.progressPercent !=
          lastFirmwareUpdateStatus_.progressPercent ||
      std::strcmp(previousStatus.version, lastFirmwareUpdateStatus_.version) !=
          0) {
    dirty_ = true;
  }

  switch (state_) {
    case AppState::OtaChecking:
      if (lastFirmwareUpdateStatus_.state == FirmwareUpdateState::Available) {
        transitionTo(AppState::OtaAvailable);
      } else if (lastFirmwareUpdateStatus_.state ==
                 FirmwareUpdateState::UpToDate) {
        transitionTo(AppState::OtaUpToDate);
      } else if (lastFirmwareUpdateStatus_.state ==
                 FirmwareUpdateState::Failed) {
        transitionTo(AppState::OtaFailed);
      } else if (lastFirmwareUpdateStatus_.state ==
                 FirmwareUpdateState::Cancelled) {
        transitionTo(AppState::OtaCancelled);
      }
      break;
    case AppState::OtaDownloading:
      if (lastFirmwareUpdateStatus_.state ==
          FirmwareUpdateState::Finalizing) {
        transitionTo(AppState::OtaFinalizing);
      } else if (lastFirmwareUpdateStatus_.state ==
                 FirmwareUpdateState::Rebooting) {
        transitionTo(AppState::OtaRebooting);
      } else if (lastFirmwareUpdateStatus_.state ==
                 FirmwareUpdateState::Failed) {
        transitionTo(AppState::OtaFailed);
      } else if (lastFirmwareUpdateStatus_.state ==
                 FirmwareUpdateState::Cancelled) {
        transitionTo(AppState::OtaCancelled);
      }
      break;
    case AppState::OtaCancelling:
      if (lastFirmwareUpdateStatus_.state == FirmwareUpdateState::Cancelled ||
          lastFirmwareUpdateStatus_.state == FirmwareUpdateState::Available ||
          lastFirmwareUpdateStatus_.state == FirmwareUpdateState::UpToDate) {
        transitionTo(AppState::OtaCancelled);
      } else if (lastFirmwareUpdateStatus_.state ==
                 FirmwareUpdateState::Failed) {
        transitionTo(AppState::OtaFailed);
      } else if (lastFirmwareUpdateStatus_.state ==
                 FirmwareUpdateState::Finalizing) {
        transitionTo(AppState::OtaFinalizing);
      } else if (lastFirmwareUpdateStatus_.state ==
                 FirmwareUpdateState::Rebooting) {
        transitionTo(AppState::OtaRebooting);
      }
      break;
    case AppState::OtaFinalizing:
      if (lastFirmwareUpdateStatus_.state == FirmwareUpdateState::Rebooting) {
        transitionTo(AppState::OtaRebooting);
      } else if (lastFirmwareUpdateStatus_.state ==
                 FirmwareUpdateState::Failed) {
        transitionTo(AppState::OtaFailed);
      } else if (lastFirmwareUpdateStatus_.state ==
                 FirmwareUpdateState::Cancelled) {
        transitionTo(AppState::OtaCancelled);
      }
      break;
    default:
      break;
  }
}

void StateMachine::requestUpload() noexcept {
  uploadStarted_ = false;
  if (!session_.isComplete() || !session_.result().has_value()) {
    lastApiResult_ = ApiResult::failure(ApiError::InvalidResponse);
    transitionTo(AppState::UploadFailed);
    return;
  }
  if (wifi_ == nullptr) {
    lastApiResult_ = ApiResult::failure(ApiError::WifiUnavailable);
    transitionTo(AppState::UploadFailed);
    return;
  }
  if (wifi_->state() != WifiState::Connected) {
    lastApiResult_ = ApiResult::failure(ApiError::Offline);
    transitionTo(AppState::UploadFailed);
    return;
  }
  if (apiClient_ == nullptr) {
    lastApiResult_ = ApiResult::failure(ApiError::TransportError);
    transitionTo(AppState::UploadFailed);
    return;
  }

  transitionTo(AppState::Uploading);
}

void StateMachine::moveHistoryCursor(int delta) noexcept {
  if (history_ == nullptr || history_->count() == 0) {
    return;
  }
  const std::size_t count = history_->count();
  const std::int64_t current =
      static_cast<std::int64_t>(historyCursor_);
  const std::int64_t next = current + delta;
  if (next < 0) {
    historyCursor_ = 0;
  } else if (static_cast<std::size_t>(next) >= count) {
    historyCursor_ = count - 1;
  } else {
    historyCursor_ = static_cast<std::size_t>(next);
  }
  dirty_ = true;
}

void StateMachine::transitionTo(AppState next) noexcept {
  if (state_ == next) {
    return;
  }
  state_ = next;
  if (next == AppState::History) {
    // Start browsing at the newest record.
    historyCursor_ = (history_ != nullptr && history_->count() > 0)
                         ? history_->count() - 1
                         : 0;
  }
  if (next == AppState::WifiFailed || next == AppState::WifiTimeout) {
    // A failure screen is entered once per failed attempt; ordinary ignored
    // button input remains silent and cannot spam the error cue.
    playSound(SoundCue::Error);
  }
  dirty_ = true;
}

void StateMachine::handleResultAction() noexcept {
  const auto& result = session_.result();
  if (result.has_value() && result->hasMovingLines()) {
    transitionTo(AppState::TransformedResult);
    return;
  }

  resetReturnState_ = AppState::HexagramResult;
  transitionTo(AppState::ResetConfirm);
}

}  // namespace jinggua::application
