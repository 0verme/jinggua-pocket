#pragma once

#include <optional>

#include "jinggua/application/divination_session.h"
#include "jinggua/domain/offline_hexagram_code.h"

namespace jinggua::application {

// Returns a code only after all six lines and the complete divination result
// exist. The code is derived from the session and is never stored separately.
std::optional<domain::OfflineHexagramCode> offlineHexagramCodeForSession(
    const DivinationSession& session) noexcept;

}  // namespace jinggua::application
