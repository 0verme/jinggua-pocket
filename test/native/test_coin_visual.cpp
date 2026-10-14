#include <array>
#include <cstddef>
#include <cstdint>

#include "test_framework.h"

#include "jinggua/domain/yao.h"
#include "jinggua/ui/coin_animation.h"
#include "jinggua/ui/coin_visual.h"
#include "jinggua/ui/layout.h"

namespace {

enum class DrawKind : std::uint8_t {
  FillEllipse,
  DrawEllipse,
  Line,
  FillRect,
};

struct DrawCall {
  DrawKind kind;
  int x0;
  int y0;
  int x1;
  int y1;
  jinggua::hardware::Color color;
};

class RecordingSurface final {
 public:
  void fillEllipse(int centerX, int centerY, int radiusX, int radiusY,
                   jinggua::hardware::Color color) noexcept {
    append({DrawKind::FillEllipse, centerX, centerY, radiusX, radiusY, color});
  }

  void drawEllipse(int centerX, int centerY, int radiusX, int radiusY,
                   jinggua::hardware::Color color) noexcept {
    append({DrawKind::DrawEllipse, centerX, centerY, radiusX, radiusY, color});
  }

  void drawLine(int x0, int y0, int x1, int y1,
                jinggua::hardware::Color color) noexcept {
    append({DrawKind::Line, x0, y0, x1, y1, color});
  }

  void fillRect(int x, int y, int width, int height,
                jinggua::hardware::Color color) noexcept {
    append({DrawKind::FillRect, x, y, width, height, color});
  }

  const DrawCall& call(std::size_t index) const noexcept { return calls_[index]; }
  std::size_t size() const noexcept { return size_; }

  std::size_t count(DrawKind kind, jinggua::hardware::Color color,
                    int width = -1, int height = -1) const noexcept {
    std::size_t matches = 0;
    for (std::size_t index = 0; index < size_; ++index) {
      const auto& current = calls_[index];
      if (current.kind == kind && current.color == color &&
          (width < 0 || current.x1 == width) &&
          (height < 0 || current.y1 == height)) {
        ++matches;
      }
    }
    return matches;
  }

 private:
  void append(const DrawCall& call) noexcept {
    if (size_ < calls_.size()) {
      calls_[size_++] = call;
    }
  }

  std::array<DrawCall, 16> calls_{};
  std::size_t size_{0};
};

}  // namespace

void runCoinVisualTests(TestRunner& runner) {
  using jinggua::domain::CoinResult;
  using jinggua::domain::CoinSide;
  using jinggua::domain::YaoType;
  using jinggua::ui::CoinAnimation;
  using jinggua::ui::CoinAnimationState;
  using jinggua::ui::coin_visual::kCopperBody;
  using jinggua::ui::coin_visual::kCopperEdge;
  using jinggua::ui::coin_visual::kCopperEngraving;
  using jinggua::ui::coin_visual::kCopperRim;
  using jinggua::ui::coin_visual::kCopperStud;
  using jinggua::ui::coin_visual::kFullWidthPercent;
  using jinggua::ui::coin_visual::kHoleSize;
  using jinggua::ui::coin_visual::kRadius;
  using jinggua::ui::coin_visual::drawCoin;
  using jinggua::ui::coin_visual::horizontalRadius;

  EXPECT_EQ(runner, kRadius, 13);
  EXPECT_EQ(runner, horizontalRadius(kFullWidthPercent), kRadius);
  EXPECT_EQ(runner, horizontalRadius(50), 6);
  EXPECT_EQ(runner, horizontalRadius(55), 7);
  EXPECT_EQ(runner, horizontalRadius(0), 2);
  EXPECT_EQ(runner, horizontalRadius(255), kRadius);
  EXPECT_EQ(runner, kHoleSize, 5);
  EXPECT(runner, kCopperBody != jinggua::ui::theme::kAccent);
  EXPECT(runner, kCopperBody != jinggua::ui::theme::kText);
  EXPECT(runner, kCopperEdge != jinggua::ui::theme::kText);
  EXPECT(runner, kCopperRim != jinggua::ui::theme::kText);
  EXPECT(runner, kCopperStud != jinggua::ui::theme::kText);

  EXPECT_EQ(runner, jinggua::ui::layout::coinCenterX(135, 0), 31);
  EXPECT_EQ(runner, jinggua::ui::layout::coinCenterX(135, 1), 67);
  EXPECT_EQ(runner, jinggua::ui::layout::coinCenterX(135, 2), 103);
  EXPECT_EQ(runner,
            jinggua::ui::layout::coinCenterX(135, 1) -
                jinggua::ui::layout::coinCenterX(135, 0),
            36);
  EXPECT_EQ(runner,
            jinggua::ui::layout::coinCenterX(135, 2) -
                jinggua::ui::layout::coinCenterX(135, 1),
            36);

  // The existing encoding is Front=3 and Back=2. Exhaust all eight ordered
  // triples, verify each maps to its original yao meaning, and then assert the
  // stopped animation renders each stored side as a full circular face.
  for (std::size_t combination = 0; combination < 8; ++combination) {
    jinggua::domain::CoinSet coins = jinggua::domain::defaultCoinSet();
    std::uint8_t frontCount = 0;
    for (std::size_t index = 0; index < coins.size(); ++index) {
      const bool front = ((combination >> index) & 1U) != 0U;
      coins[index] = front ? CoinSide::Front : CoinSide::Back;
      if (front) {
        ++frontCount;
      }
    }

    const auto result = CoinResult::fromCoins(coins);
    EXPECT_EQ(runner, result.total,
              static_cast<std::uint8_t>(6U + frontCount));
    const auto yao = jinggua::domain::createYao(1, result);
    EXPECT(runner, yao.has_value());
    EXPECT_EQ(runner, yao->position, static_cast<std::uint8_t>(1));

    const YaoType expectedType =
        frontCount == 0U   ? YaoType::OldYin
        : frontCount == 1U ? YaoType::YoungYang
        : frontCount == 2U ? YaoType::YoungYin
                           : YaoType::OldYang;
    EXPECT_EQ(runner, yao->type, expectedType);
    EXPECT_EQ(runner, yao->moving,
              frontCount == 0U || frontCount == 3U);

    CoinAnimation animation;
    constexpr std::uint32_t startMs = 1000;
    animation.start(result, startMs);
    EXPECT(runner, animation.update(startMs + CoinAnimation::kDurationMs));
    EXPECT_EQ(runner, animation.state(), CoinAnimationState::Finished);

    for (std::size_t index = 0; index < coins.size(); ++index) {
      const auto& frame = animation.frame(index);
      EXPECT_EQ(runner, frame.side, coins[index]);
      EXPECT_EQ(runner, frame.widthPercent, kFullWidthPercent);

      RecordingSurface surface;
      const int centerX = jinggua::ui::layout::coinCenterX(135, index);
      drawCoin(surface, frame.side, centerX, 68, frame.widthPercent);
      EXPECT_EQ(runner, surface.size(),
                frame.side == CoinSide::Front ? static_cast<std::size_t>(9)
                                              : static_cast<std::size_t>(7));
      EXPECT_EQ(runner, surface.call(0).kind, DrawKind::FillEllipse);
      EXPECT_EQ(runner, surface.call(0).x0, centerX);
      EXPECT_EQ(runner, surface.call(0).x1, kRadius);
      EXPECT_EQ(runner, surface.call(0).y1, kRadius);
      EXPECT_EQ(runner, surface.call(0).color, kCopperBody);
      EXPECT_EQ(runner, surface.call(1).kind, DrawKind::DrawEllipse);
      EXPECT_EQ(runner, surface.call(1).x1, kRadius);
      EXPECT_EQ(runner, surface.call(1).y1, kRadius);
      EXPECT_EQ(runner, surface.call(1).color, kCopperEdge);
      EXPECT_EQ(runner, surface.call(2).kind, DrawKind::DrawEllipse);
      EXPECT_EQ(runner, surface.call(2).x1, kRadius - 2);
      EXPECT_EQ(runner, surface.call(2).y1, kRadius - 2);
      EXPECT_EQ(runner, surface.count(DrawKind::FillRect,
                                      jinggua::ui::theme::kBackground,
                                      kHoleSize, kHoleSize),
                static_cast<std::size_t>(1));
      const auto& hole = surface.call(surface.size() - 1U);
      EXPECT_EQ(runner, hole.kind, DrawKind::FillRect);
      EXPECT_EQ(runner, hole.x0, centerX - kHoleSize / 2);
      EXPECT_EQ(runner, hole.y0, 68 - kHoleSize / 2);

      if (frame.side == CoinSide::Front) {
        EXPECT_EQ(runner, surface.count(DrawKind::FillEllipse, kCopperStud, 1,
                                        1),
                  static_cast<std::size_t>(4));
        EXPECT_EQ(runner, surface.count(DrawKind::Line, kCopperEngraving),
                  static_cast<std::size_t>(0));
      } else {
        EXPECT_EQ(runner, surface.count(DrawKind::FillEllipse, kCopperStud, 1,
                                        1),
                  static_cast<std::size_t>(0));
        EXPECT_EQ(runner, surface.count(DrawKind::Line, kCopperEngraving),
                  static_cast<std::size_t>(2));
      }
    }
  }

  RecordingSurface edgeOnSurface;
  drawCoin(edgeOnSurface, CoinSide::Front, 67, 88, 50);
  EXPECT_EQ(runner, edgeOnSurface.call(0).x1, 6);
  EXPECT_EQ(runner, edgeOnSurface.call(0).y1, kRadius);
  EXPECT_EQ(runner, edgeOnSurface.count(DrawKind::FillRect,
                                       jinggua::ui::theme::kBackground,
                                       kHoleSize, kHoleSize),
            static_cast<std::size_t>(0));
}
