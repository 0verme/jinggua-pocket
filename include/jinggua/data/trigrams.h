#pragma once

#include <array>

#include "jinggua/domain/trigram.h"

namespace jinggua::data {

const std::array<domain::Trigram, 8>& allTrigrams() noexcept;
const domain::Trigram* findTrigram(domain::TrigramId id) noexcept;

}  // namespace jinggua::data
