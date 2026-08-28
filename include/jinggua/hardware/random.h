#pragma once

#include "jinggua/application/random_provider.h"

namespace jinggua::hardware {

class Esp32RandomProvider final : public application::RandomProvider {
 public:
  domain::CoinSide tossCoin() override;
};

}  // namespace jinggua::hardware
