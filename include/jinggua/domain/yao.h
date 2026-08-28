#pragma once

#include <cstdint>
#include <optional>

#include "jinggua/domain/coin.h"

namespace jinggua::domain {

enum class YinYang : std::uint8_t {
  Yin,
  Yang,
};

enum class YaoType : std::uint8_t {
  OldYin,
  YoungYang,
  YoungYin,
  OldYang,
};

struct Yao {
  std::uint8_t position{0};
  CoinResult coins{};
  YaoType type{YaoType::YoungYin};
  YinYang yinYang{YinYang::Yin};
  bool moving{false};
  YinYang transformedYinYang{YinYang::Yin};
};

std::optional<Yao> createYao(std::uint8_t position,
                             const CoinResult& coins) noexcept;
YaoType yaoTypeForTotal(std::uint8_t total) noexcept;
YinYang yinYangFor(YaoType type) noexcept;
YinYang transformedYinYangFor(YaoType type) noexcept;
bool isMoving(YaoType type) noexcept;
const char* yaoTypeName(YaoType type) noexcept;
const char* yinYangName(YinYang value) noexcept;

}  // namespace jinggua::domain
