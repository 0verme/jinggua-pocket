#pragma once

#include "jinggua/domain/coin.h"

namespace jinggua::application {

class RandomProvider {
 public:
  virtual ~RandomProvider() = default;
  virtual domain::CoinSide tossCoin() = 0;
};

}  // namespace jinggua::application
