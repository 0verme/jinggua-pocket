#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace jinggua::domain {

using OfflineHexagramCode = std::uint16_t;
constexpr OfflineHexagramCode kOfflineHexagramCodeMaximum = 4095;

// JingGua Offline Hexagram Code V1. lineValues are six complete yao totals,
// ordered from the first line (bottom) to the sixth line (top), each in 6..9.
std::optional<OfflineHexagramCode> encodeOfflineHexagramCode(
    const std::array<std::uint8_t, 6>& lineValues) noexcept;

// Writes the code as exactly four zero-padded decimal digits plus a null
// terminator. Returns false for an invalid code, null output, or a short
// output buffer.
bool formatOfflineHexagramCode(OfflineHexagramCode code, char* output,
                               std::size_t outputSize) noexcept;

}  // namespace jinggua::domain
