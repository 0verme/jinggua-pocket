#pragma once

#include <cstddef>
#include <cstdint>

namespace jinggua::application {

constexpr std::size_t kFirmwareVersionMaxLength = 24;
constexpr std::size_t kFirmwareUrlMaxLength = 192;
constexpr std::size_t kFirmwareManifestMaxBytes = 512;
constexpr std::size_t kFirmwareSha256HexLength = 64;
constexpr std::uint32_t kFirmwareOtaSlotMaxBytes = 0x330000U;

#ifndef JINGGUA_FIRMWARE_VERSION
#define JINGGUA_FIRMWARE_VERSION "0.3.0"
#endif

inline constexpr const char* kJingGuaFirmwareVersion =
    JINGGUA_FIRMWARE_VERSION;

struct FirmwareUpdateManifest {
  char version[kFirmwareVersionMaxLength + 1]{};
  char firmwareUrl[kFirmwareUrlMaxLength + 1]{};
  char sha256[kFirmwareSha256HexLength + 1]{};
  std::uint32_t sizeBytes{0};
};

enum class FirmwareManifestError : std::uint8_t {
  None,
  InvalidArgument,
  TooLarge,
  InvalidField,
  MissingField,
  InvalidVersion,
  InvalidSize,
  InvalidSha256,
  InvalidUrl,
};

// Parses the bounded, line-oriented manifest distributed as a GitHub Release
// asset. Unknown keys, duplicates, malformed values and non-GitHub firmware
// URLs are rejected before any flash partition is erased.
bool parseFirmwareUpdateManifest(const char* data, std::size_t size,
                                 FirmwareUpdateManifest& out,
                                 FirmwareManifestError* error = nullptr) noexcept;

// Numeric semantic versions only (major.minor.patch, optional leading v).
// Returns false for malformed input; comparison is -1, 0, or 1.
bool compareFirmwareVersions(const char* left, const char* right,
                             int& comparison) noexcept;

const char* firmwareManifestErrorName(FirmwareManifestError error) noexcept;

enum class FirmwareUpdateState : std::uint8_t {
  Idle,
  Checking,
  UpToDate,
  Available,
  Downloading,
  Finalizing,
  Rebooting,
  Cancelled,
  Failed,
};

enum class FirmwareUpdateError : std::uint8_t {
  None,
  Offline,
  NotConfigured,
  Network,
  InvalidManifest,
  InvalidVersion,
  NoSpace,
  DownloadInterrupted,
  HashMismatch,
  InvalidImage,
  ActivationFailed,
  ResourceUnavailable,
};

struct FirmwareUpdateStatus {
  FirmwareUpdateState state{FirmwareUpdateState::Idle};
  FirmwareUpdateError error{FirmwareUpdateError::None};
  std::uint8_t progressPercent{0};
  char version[kFirmwareVersionMaxLength + 1]{};
};

const char* firmwareUpdateStateName(FirmwareUpdateState state) noexcept;
const char* firmwareUpdateErrorName(FirmwareUpdateError error) noexcept;

// Non-blocking port for a user-triggered update. Implementations must use a
// one-shot operation, report bounded progress, and preserve the running image
// until a complete, verified replacement is ready to boot. Cancellation must
// be atomically closed before activating the verified replacement.
class FirmwareUpdateClient {
 public:
  virtual ~FirmwareUpdateClient() = default;

  virtual bool beginCheck() noexcept = 0;
  virtual bool beginInstall() noexcept = 0;
  virtual void cancel() noexcept = 0;
  virtual FirmwareUpdateStatus status() const noexcept = 0;
};

}  // namespace jinggua::application
