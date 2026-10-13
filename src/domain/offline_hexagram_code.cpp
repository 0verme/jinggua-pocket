#include "jinggua/domain/offline_hexagram_code.h"

namespace jinggua::domain {

std::optional<OfflineHexagramCode> encodeOfflineHexagramCode(
    const std::array<std::uint8_t, 6>& lineValues) noexcept {
  std::uint32_t code = 0;
  std::uint32_t placeValue = 1;
  for (const auto lineValue : lineValues) {
    if (lineValue < 6 || lineValue > 9) {
      return std::nullopt;
    }
    code += static_cast<std::uint32_t>(lineValue - 6) * placeValue;
    placeValue *= 4;
  }
  return static_cast<OfflineHexagramCode>(code);
}

bool formatOfflineHexagramCode(OfflineHexagramCode code, char* output,
                               std::size_t outputSize) noexcept {
  if (output == nullptr || outputSize < 5 ||
      code > kOfflineHexagramCodeMaximum) {
    return false;
  }

  output[0] = static_cast<char>('0' + (code / 1000U));
  output[1] = static_cast<char>('0' + ((code / 100U) % 10U));
  output[2] = static_cast<char>('0' + ((code / 10U) % 10U));
  output[3] = static_cast<char>('0' + (code % 10U));
  output[4] = '\0';
  return true;
}

}  // namespace jinggua::domain
