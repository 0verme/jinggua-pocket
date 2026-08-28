#include "jinggua/domain/yao.h"

namespace jinggua::domain {

std::optional<Yao> createYao(std::uint8_t position,
                             const CoinResult& coins) noexcept {
  if (position < 1 || position > 6 || coins.total < 6 || coins.total > 9) {
    return std::nullopt;
  }

  const auto type = yaoTypeForTotal(coins.total);
  return Yao{position,
             coins,
             type,
             yinYangFor(type),
             isMoving(type),
             transformedYinYangFor(type)};
}

YaoType yaoTypeForTotal(std::uint8_t total) noexcept {
  switch (total) {
    case 6:
      return YaoType::OldYin;
    case 7:
      return YaoType::YoungYang;
    case 9:
      return YaoType::OldYang;
    case 8:
    default:
      return YaoType::YoungYin;
  }
}

YinYang yinYangFor(YaoType type) noexcept {
  switch (type) {
    case YaoType::OldYin:
    case YaoType::YoungYin:
      return YinYang::Yin;
    case YaoType::YoungYang:
    case YaoType::OldYang:
      return YinYang::Yang;
  }
  return YinYang::Yin;
}

YinYang transformedYinYangFor(YaoType type) noexcept {
  if (!isMoving(type)) {
    return yinYangFor(type);
  }
  return yinYangFor(type) == YinYang::Yin ? YinYang::Yang : YinYang::Yin;
}

bool isMoving(YaoType type) noexcept {
  return type == YaoType::OldYin || type == YaoType::OldYang;
}

const char* yaoTypeName(YaoType type) noexcept {
  switch (type) {
    case YaoType::OldYin:
      return "老阴";
    case YaoType::YoungYang:
      return "少阳";
    case YaoType::YoungYin:
      return "少阴";
    case YaoType::OldYang:
      return "老阳";
  }
  return "未知";
}

const char* yinYangName(YinYang value) noexcept {
  return value == YinYang::Yin ? "阴" : "阳";
}

}  // namespace jinggua::domain
