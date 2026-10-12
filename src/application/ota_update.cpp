#include "jinggua/application/ota_update.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace jinggua::application {
namespace {

struct VersionParts {
  std::uint32_t values[3]{};
};

bool parseUnsigned(const char* text, std::size_t length,
                   std::uint32_t& value) noexcept {
  if (text == nullptr || length == 0 ||
      (length > 1 && text[0] == '0')) {
    return false;
  }

  std::uint32_t parsed = 0;
  for (std::size_t index = 0; index < length; ++index) {
    const char digit = text[index];
    if (digit < '0' || digit > '9') {
      return false;
    }
    const auto number = static_cast<std::uint32_t>(digit - '0');
    if (parsed > (std::numeric_limits<std::uint32_t>::max() - number) / 10U) {
      return false;
    }
    parsed = parsed * 10U + number;
  }
  value = parsed;
  return true;
}

bool parseVersion(const char* text, VersionParts& version) noexcept {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  if (*text == 'v' || *text == 'V') {
    ++text;
  }

  const char* partStart = text;
  std::size_t partIndex = 0;
  for (const char* cursor = text;; ++cursor) {
    if (*cursor == '.' || *cursor == '\0') {
      if (partIndex >= 3 ||
          !parseUnsigned(partStart,
                         static_cast<std::size_t>(cursor - partStart),
                         version.values[partIndex])) {
        return false;
      }
      ++partIndex;
      if (*cursor == '\0') {
        return partIndex == 3;
      }
      partStart = cursor + 1;
    }
  }
}

bool isHexDigit(char value) noexcept {
  return (value >= '0' && value <= '9') ||
         (value >= 'a' && value <= 'f') ||
         (value >= 'A' && value <= 'F');
}

char lowercaseHex(char value) noexcept {
  if (value >= 'A' && value <= 'F') {
    return static_cast<char>(value - 'A' + 'a');
  }
  return value;
}

FirmwareManifestError fail(FirmwareManifestError error,
                           FirmwareManifestError* output) noexcept {
  if (output != nullptr) {
    *output = error;
  }
  return error;
}

bool parseSize(const char* text, std::size_t length,
               std::uint32_t& size) noexcept {
  return parseUnsigned(text, length, size) && size > 0 &&
         size <= kFirmwareOtaSlotMaxBytes;
}

bool validFirmwareUrl(const char* url, std::size_t length,
                      const char* version,
                      std::size_t versionLength) noexcept {
  constexpr char kPrefix[] =
      "https://github.com/0verme/jinggua-pocket/releases/download/v";
  constexpr char kSuffix[] = "/firmware.bin";
  const std::size_t prefixLength = sizeof(kPrefix) - 1;
  const std::size_t suffixLength = sizeof(kSuffix) - 1;
  if (length != prefixLength + versionLength + suffixLength ||
      std::memcmp(url, kPrefix, prefixLength) != 0 ||
      std::memcmp(url + prefixLength, version, versionLength) != 0 ||
      std::memcmp(url + prefixLength + versionLength, kSuffix,
                  suffixLength) != 0) {
    return false;
  }
  return true;
}

}  // namespace

bool parseFirmwareUpdateManifest(const char* data, std::size_t size,
                                 FirmwareUpdateManifest& out,
                                 FirmwareManifestError* error) noexcept {
  out = FirmwareUpdateManifest{};
  if (error != nullptr) {
    *error = FirmwareManifestError::None;
  }
  if (data == nullptr || size == 0) {
    fail(FirmwareManifestError::InvalidArgument, error);
    return false;
  }
  if (size > kFirmwareManifestMaxBytes) {
    fail(FirmwareManifestError::TooLarge, error);
    return false;
  }

  bool hasFormat = false;
  bool hasVersion = false;
  bool hasSize = false;
  bool hasSha256 = false;
  bool hasUrl = false;
  std::size_t position = 0;
  while (position < size) {
    const std::size_t lineStart = position;
    while (position < size && data[position] != '\n') {
      if (data[position] == '\0') {
        fail(FirmwareManifestError::InvalidField, error);
        return false;
      }
      ++position;
    }
    std::size_t lineLength = position - lineStart;
    if (position < size) {
      ++position;
    }
    if (lineLength > 0 && data[lineStart + lineLength - 1] == '\r') {
      --lineLength;
    }
    if (lineLength == 0) {
      if (position == size && lineStart + lineLength < size) {
        continue;
      }
      fail(FirmwareManifestError::InvalidField, error);
      return false;
    }

    std::size_t separator = 0;
    while (separator < lineLength && data[lineStart + separator] != '=') {
      ++separator;
    }
    if (separator == 0 || separator == lineLength) {
      fail(FirmwareManifestError::InvalidField, error);
      return false;
    }
    const char* key = data + lineStart;
    const char* value = data + lineStart + separator + 1;
    const std::size_t valueLength = lineLength - separator - 1;

    if (separator == 6 && std::memcmp(key, "format", 6) == 0) {
      if (hasFormat || valueLength != 1 || value[0] != '1') {
        fail(FirmwareManifestError::InvalidField, error);
        return false;
      }
      hasFormat = true;
    } else if (separator == 7 && std::memcmp(key, "version", 7) == 0) {
      if (hasVersion || valueLength == 0 ||
          valueLength > kFirmwareVersionMaxLength) {
        fail(FirmwareManifestError::InvalidVersion, error);
        return false;
      }
      std::memcpy(out.version, value, valueLength);
      out.version[valueLength] = '\0';
      VersionParts parsed;
      if (!parseVersion(out.version, parsed) || out.version[0] == 'v' ||
          out.version[0] == 'V') {
        fail(FirmwareManifestError::InvalidVersion, error);
        return false;
      }
      hasVersion = true;
    } else if (separator == 4 && std::memcmp(key, "size", 4) == 0) {
      if (hasSize || !parseSize(value, valueLength, out.sizeBytes)) {
        fail(FirmwareManifestError::InvalidSize, error);
        return false;
      }
      hasSize = true;
    } else if (separator == 6 && std::memcmp(key, "sha256", 6) == 0) {
      if (hasSha256 || valueLength != kFirmwareSha256HexLength) {
        fail(FirmwareManifestError::InvalidSha256, error);
        return false;
      }
      for (std::size_t index = 0; index < valueLength; ++index) {
        if (!isHexDigit(value[index])) {
          fail(FirmwareManifestError::InvalidSha256, error);
          return false;
        }
        out.sha256[index] = lowercaseHex(value[index]);
      }
      out.sha256[kFirmwareSha256HexLength] = '\0';
      hasSha256 = true;
    } else if (separator == 3 && std::memcmp(key, "url", 3) == 0) {
      if (hasUrl || valueLength == 0 ||
          valueLength > kFirmwareUrlMaxLength) {
        fail(FirmwareManifestError::InvalidUrl, error);
        return false;
      }
      std::memcpy(out.firmwareUrl, value, valueLength);
      out.firmwareUrl[valueLength] = '\0';
      hasUrl = true;
    } else {
      fail(FirmwareManifestError::InvalidField, error);
      return false;
    }
  }

  if (!hasFormat || !hasVersion || !hasSize || !hasSha256 || !hasUrl) {
    fail(FirmwareManifestError::MissingField, error);
    return false;
  }
  if (!validFirmwareUrl(out.firmwareUrl, std::strlen(out.firmwareUrl),
                        out.version, std::strlen(out.version))) {
    fail(FirmwareManifestError::InvalidUrl, error);
    return false;
  }
  return true;
}

bool compareFirmwareVersions(const char* left, const char* right,
                             int& comparison) noexcept {
  VersionParts leftVersion;
  VersionParts rightVersion;
  if (!parseVersion(left, leftVersion) || !parseVersion(right, rightVersion)) {
    return false;
  }
  comparison = 0;
  for (std::size_t index = 0; index < 3; ++index) {
    if (leftVersion.values[index] < rightVersion.values[index]) {
      comparison = -1;
      return true;
    }
    if (leftVersion.values[index] > rightVersion.values[index]) {
      comparison = 1;
      return true;
    }
  }
  return true;
}

const char* firmwareManifestErrorName(FirmwareManifestError error) noexcept {
  switch (error) {
    case FirmwareManifestError::None:
      return "NONE";
    case FirmwareManifestError::InvalidArgument:
      return "INVALID_ARGUMENT";
    case FirmwareManifestError::TooLarge:
      return "TOO_LARGE";
    case FirmwareManifestError::InvalidField:
      return "INVALID_FIELD";
    case FirmwareManifestError::MissingField:
      return "MISSING_FIELD";
    case FirmwareManifestError::InvalidVersion:
      return "INVALID_VERSION";
    case FirmwareManifestError::InvalidSize:
      return "INVALID_SIZE";
    case FirmwareManifestError::InvalidSha256:
      return "INVALID_SHA256";
    case FirmwareManifestError::InvalidUrl:
      return "INVALID_URL";
  }
  return "UNKNOWN";
}

const char* firmwareUpdateStateName(FirmwareUpdateState state) noexcept {
  switch (state) {
    case FirmwareUpdateState::Idle:
      return "IDLE";
    case FirmwareUpdateState::Checking:
      return "CHECKING";
    case FirmwareUpdateState::UpToDate:
      return "UP_TO_DATE";
    case FirmwareUpdateState::Available:
      return "AVAILABLE";
    case FirmwareUpdateState::Downloading:
      return "DOWNLOADING";
    case FirmwareUpdateState::Rebooting:
      return "REBOOTING";
    case FirmwareUpdateState::Cancelled:
      return "CANCELLED";
    case FirmwareUpdateState::Failed:
      return "FAILED";
  }
  return "UNKNOWN";
}

const char* firmwareUpdateErrorName(FirmwareUpdateError error) noexcept {
  switch (error) {
    case FirmwareUpdateError::None:
      return "NONE";
    case FirmwareUpdateError::Offline:
      return "OFFLINE";
    case FirmwareUpdateError::NotConfigured:
      return "NOT_CONFIGURED";
    case FirmwareUpdateError::Network:
      return "NETWORK";
    case FirmwareUpdateError::InvalidManifest:
      return "INVALID_MANIFEST";
    case FirmwareUpdateError::InvalidVersion:
      return "INVALID_VERSION";
    case FirmwareUpdateError::NoSpace:
      return "NO_SPACE";
    case FirmwareUpdateError::DownloadInterrupted:
      return "DOWNLOAD_INTERRUPTED";
    case FirmwareUpdateError::HashMismatch:
      return "HASH_MISMATCH";
    case FirmwareUpdateError::InvalidImage:
      return "INVALID_IMAGE";
    case FirmwareUpdateError::ActivationFailed:
      return "ACTIVATION_FAILED";
    case FirmwareUpdateError::ResourceUnavailable:
      return "RESOURCE_UNAVAILABLE";
  }
  return "UNKNOWN";
}

}  // namespace jinggua::application
