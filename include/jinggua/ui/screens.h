#pragma once

#include "jinggua/application/state_machine.h"
#include "jinggua/application/wifi_controller.h"
#include "jinggua/hardware/display.h"

namespace jinggua::ui {

class CoinAnimation;

void renderWelcome(hardware::Display& display) noexcept;
void renderPrepare(hardware::Display& display) noexcept;
void renderCasting(hardware::Display& display,
                   const application::DivinationSession& session) noexcept;
void renderLineResult(
    hardware::Display& display,
    const application::DivinationSession& session,
    const CoinAnimation* animation = nullptr) noexcept;
void renderHexagramResult(
    hardware::Display& display,
    const application::DivinationSession& session) noexcept;
void renderTransformedResult(
    hardware::Display& display,
    const application::DivinationSession& session) noexcept;
void renderResetConfirm(hardware::Display& display) noexcept;
void renderWifiSettings(hardware::Display& display,
                        const application::WifiController& wifi,
                        bool soundEnabled) noexcept;
void renderWifiConnecting(hardware::Display& display) noexcept;
void renderWifiConnected(hardware::Display& display,
                         const application::WifiController& wifi) noexcept;
void renderWifiFailed(hardware::Display& display,
                      const application::WifiController& wifi) noexcept;
void renderWifiTimeout(hardware::Display& display) noexcept;
void renderUploading(hardware::Display& display) noexcept;
void renderUploadSuccess(hardware::Display& display) noexcept;
void renderUploadFailed(hardware::Display& display) noexcept;

}  // namespace jinggua::ui
