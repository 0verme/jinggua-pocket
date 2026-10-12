#pragma once

#include <cstddef>

#include "jinggua/application/ota_update.h"
#include "jinggua/ui/layout.h"

namespace jinggua::ui {

static_assert(application::kFirmwareVersionMaxLength <=
              layout::kFirmwareVersionCharactersPerLine * 2,
              "Firmware version display must fit the supported version length");

struct FirmwareVersionLines {
  char first[layout::kFirmwareVersionCharactersPerLine + 1]{};
  char second[layout::kFirmwareVersionCharactersPerLine + 1]{};
};

// Split the complete bounded OTA version across two lines without dropping a
// suffix. OTA versions are ASCII semantic versions, so byte and character
// counts are identical.
inline bool splitFirmwareVersion(const char* version,
                                 FirmwareVersionLines& lines) noexcept {
  lines = {};
  if (version == nullptr || version[0] == '\0') {
    return false;
  }

  std::size_t index = 0;
  for (; index < application::kFirmwareVersionMaxLength &&
         version[index] != '\0';
       ++index) {
    if (index < layout::kFirmwareVersionCharactersPerLine) {
      lines.first[index] = version[index];
    } else {
      lines.second[index - layout::kFirmwareVersionCharactersPerLine] =
          version[index];
    }
  }
  return version[index] == '\0';
}

}  // namespace jinggua::ui
