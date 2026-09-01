#pragma once

#include <cstdint>

namespace jinggua::application {

// Device Wi-Fi radio state. The radio is Off by default (offline-first) and
// only moves to Connecting after the user explicitly requests a connection.
enum class WifiState {
  Off,
  Connecting,
  Connected,
  Failed,
  Timeout,
};

const char* wifiStateName(WifiState state) noexcept;

// Port for the application layer. StateMachine only depends on this interface,
// so native tests can inject a fake controller and the StickS3 adapter stays
// behind the same boundary.
class WifiController {
 public:
  virtual ~WifiController() = default;

  // Current radio state.
  virtual WifiState state() const noexcept = 0;
  // Start a non-blocking connection attempt. Returns false when the controller
  // cannot begin connecting (e.g. no credentials configured); the caller then
  // shows a failure screen instead of a connecting screen.
  virtual bool enable() noexcept = 0;
  // Explicitly turn the radio off. Never called automatically by the app.
  virtual void disable() noexcept = 0;
  // Advance the connection attempt. Call from the main loop while connecting
  // so connection progress never blocks the loop. Returns the current state.
  virtual WifiState update(std::uint32_t nowMs) noexcept = 0;
  // Configured network name, for display only. Never exposes the password.
  virtual const char* ssid() const noexcept { return ""; }
  // True when credentials are configured; otherwise connecting cannot succeed.
  virtual bool configured() const noexcept { return false; }
};

}  // namespace jinggua::application
