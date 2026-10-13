#include "jinggua/application/offline_hexagram_code.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace jinggua::application {

std::optional<domain::OfflineHexagramCode> offlineHexagramCodeForSession(
    const DivinationSession& session) noexcept {
  if (!session.isComplete() || !session.result().has_value()) {
    return std::nullopt;
  }

  std::array<std::uint8_t, DivinationSession::kLineCount> lineValues{};
  for (std::size_t index = 0; index < lineValues.size(); ++index) {
    lineValues[index] = session.lines()[index].coins.total;
  }
  return domain::encodeOfflineHexagramCode(lineValues);
}

}  // namespace jinggua::application
