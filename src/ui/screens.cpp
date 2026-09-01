#include "jinggua/ui/screens.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "jinggua/ui/coin_animation.h"
#include "jinggua/ui/layout.h"
#include "jinggua/ui/theme.h"
#include "jinggua/ui/typography.h"

namespace jinggua::ui {
namespace {

int margin(const hardware::Display& display) noexcept {
  return layout::horizontalMargin(display.width());
}

void clearScreen(hardware::Display& display) noexcept {
  display.clear(theme::kBackground);
}

void drawTitle(hardware::Display& display, const char* title) noexcept {
  display.drawText(title, margin(display), layout::kHeaderY, theme::kAccent,
                   typography::kSection);
}

void drawFooter(hardware::Display& display, const char* primary,
                const char* secondary = nullptr) noexcept {
  if (secondary != nullptr) {
    display.drawText(secondary, margin(display),
                     layout::footerSecondaryY(display.height()),
                     theme::kMuted, typography::kFooter);
  }
  display.drawText(primary, margin(display), layout::footerY(display.height()),
                   theme::kMuted, typography::kFooter);
}

const char* coinSymbol(domain::CoinSide side) noexcept {
  return side == domain::CoinSide::Front ? "●" : "○";
}

const char* linePositionName(std::uint8_t position) noexcept {
  switch (position) {
    case 1:
      return "一";
    case 2:
      return "二";
    case 3:
      return "三";
    case 4:
      return "四";
    case 5:
      return "五";
    case 6:
      return "六";
    default:
      return "?";
  }
}

std::size_t utf8SequenceLength(const char* text) noexcept {
  if (text == nullptr || text[0] == '\0') {
    return 0;
  }
  const auto first = static_cast<unsigned char>(text[0]);
  if ((first & 0x80U) == 0U) {
    return 1;
  }
  if ((first & 0xE0U) == 0xC0U) {
    return 2;
  }
  if ((first & 0xF0U) == 0xE0U) {
    return 3;
  }
  if ((first & 0xF8U) == 0xF0U) {
    return 4;
  }
  return 1;
}

void drawSsid(hardware::Display& display,
              const application::WifiController& wifi) noexcept {
  const char* source = wifi.ssid();
  char shortened[layout::kWifiSsidMaxCharacters * 4 + 4]{};
  std::size_t sourceOffset = 0;
  std::size_t outputOffset = 0;
  std::size_t characterCount = 0;
  while (source != nullptr && source[sourceOffset] != '\0' &&
         characterCount < layout::kWifiSsidMaxCharacters) {
    std::size_t sequenceLength = utf8SequenceLength(source + sourceOffset);
    bool completeSequence = sequenceLength > 0;
    for (std::size_t index = 1; index < sequenceLength; ++index) {
      const auto byte = static_cast<unsigned char>(
          source[sourceOffset + index]);
      if (byte == 0 || (byte & 0xC0U) != 0x80U) {
        completeSequence = false;
        break;
      }
    }
    if (!completeSequence) {
      sequenceLength = 1;
    }
    if (outputOffset + sequenceLength + 3 >= sizeof(shortened)) {
      break;
    }
    for (std::size_t index = 0; index < sequenceLength; ++index) {
      shortened[outputOffset++] = source[sourceOffset++];
    }
    ++characterCount;
  }

  if (source != nullptr && source[sourceOffset] != '\0') {
    const char* ellipsis = "…";
    for (std::size_t index = 0; ellipsis[index] != '\0'; ++index) {
      shortened[outputOffset++] = ellipsis[index];
    }
  }
  display.drawText(shortened, margin(display), layout::kWifiSsidY,
                   theme::kMuted, typography::kSecondary);
}

void drawCoinPrompt(hardware::Display& display) noexcept {
  const int centerY = layout::kCastingCoinsY;
  for (std::size_t index = 0; index < 3; ++index) {
    const int centerX = layout::coinCenterX(display.width(), index);
    display.fillEllipse(centerX, centerY, layout::kCoinPromptRadiusX,
                        layout::kCoinPromptRadiusY, theme::kAccent);
    display.drawEllipse(centerX, centerY, layout::kCoinPromptRadiusX,
                        layout::kCoinPromptRadiusY, theme::kText);
    display.drawText("●", centerX - layout::kCoinPromptSymbolOffset,
                     centerY - layout::kCoinPromptSymbolOffset,
                     theme::kBackground, typography::kSecondary);
  }
}

void drawCoinResult(hardware::Display& display,
                    const domain::CoinResult& result) noexcept {
  const int centerY = layout::kLineResultCoinsY;
  for (std::size_t index = 0; index < result.coins.size(); ++index) {
    const int centerX = layout::coinCenterX(display.width(), index);
    display.fillEllipse(centerX, centerY, layout::kCoinResultRadiusX,
                        layout::kCoinResultRadiusY, theme::kAccent);
    display.drawEllipse(centerX, centerY, layout::kCoinResultRadiusX,
                        layout::kCoinResultRadiusY, theme::kText);
    display.drawText(coinSymbol(result.coins[index]),
                     centerX - layout::kCoinSymbolHalfWidth,
                     centerY - layout::kCoinResultSymbolYOffset,
                     theme::kBackground, typography::kCoin);
  }
}

void drawHexagram(hardware::Display& display,
                 const domain::Hexagram& hexagram, int top) noexcept {
  const int lineSpacing = layout::hexagramLineSpacing(display.height());
  const int lineWidth = layout::hexagramLineWidth(display.width());
  const int gap = lineWidth > layout::kHexagramYinGap
                      ? layout::kHexagramYinGap
                      : lineWidth / 3;
  const int x = layout::hexagramLeft(display.width());
  const int markerX = x + lineWidth + layout::kHexagramMarkerGap;

  for (int screenIndex = 0; screenIndex < 6; ++screenIndex) {
    const std::size_t lineIndex = 5U - static_cast<std::size_t>(screenIndex);
    const int y = top + screenIndex * lineSpacing;
    if (hexagram.pattern[lineIndex] == domain::YinYang::Yang) {
      display.drawLine(x, y, x + lineWidth, y, theme::kText);
    } else {
      const int segment = (lineWidth - gap) / 2;
      display.drawLine(x, y, x + segment, y, theme::kText);
      display.drawLine(x + segment + gap, y, x + lineWidth, y,
                       theme::kText);
    }
    if (hexagram.lines[lineIndex].moving) {
      display.drawText("○", markerX, y - layout::kHexagramMarkerYOffset,
                       theme::kAccent, typography::kStatus);
    }
  }
}

void drawMovingSummary(hardware::Display& display,
                       const domain::DivinationResult& result, int y) noexcept {
  if (!result.hasMovingLines()) {
    display.drawText("无动爻", margin(display), y, theme::kAccent,
                     typography::kStatus);
    return;
  }

  char moving[64]{};
  std::size_t offset = static_cast<std::size_t>(
      std::snprintf(moving, sizeof(moving), "动："));
  for (std::uint8_t index = 0; index < result.movingCount; ++index) {
    if (offset >= sizeof(moving)) {
      break;
    }
    const int written = std::snprintf(
        moving + offset, sizeof(moving) - offset, "%s%s",
        index == 0 ? "" : "、",
        linePositionName(result.movingPositions[index]));
    if (written <= 0) {
      break;
    }
    offset += static_cast<std::size_t>(written);
  }
  display.drawText(moving, margin(display), y, theme::kAccent,
                   typography::kStatus);
}

void drawLineHeading(hardware::Display& display, std::uint8_t position) noexcept {
  char heading[24]{};
  std::snprintf(heading, sizeof(heading), "第 %u 爻",
                static_cast<unsigned>(position));
  drawTitle(display, heading);
}

int drawResultHeader(hardware::Display& display, const char* label,
                     const domain::Hexagram& hexagram) noexcept {
  clearScreen(display);
  drawTitle(display, label);
  display.drawText(hexagram.name, margin(display), layout::kResultNameY,
                   theme::kText, typography::kPrimary);

  char number[24]{};
  std::snprintf(number, sizeof(number), "第 %u 卦",
                static_cast<unsigned>(hexagram.number));
  display.drawText(number, margin(display), layout::kResultNumberY,
                   theme::kMuted, typography::kSecondary);
  return layout::kHexagramTop;
}

}  // namespace

void renderWelcome(hardware::Display& display) noexcept {
  clearScreen(display);
  display.drawText("静卦", margin(display), layout::kHomeTitleY,
                   theme::kAccent, typography::kTitle);
  display.drawText("摇一摇", margin(display), layout::kHomePrimaryY,
                   theme::kText, typography::kPrimary);
  display.drawText("或按 A", margin(display), layout::kHomeSecondaryY,
                   theme::kMuted, typography::kSecondary);
  drawFooter(display, "B 设置");
}

void renderPrepare(hardware::Display& display) noexcept {
  clearScreen(display);
  drawTitle(display, "准备");
  display.drawText("默念所问", margin(display), layout::kPreparePrimaryY,
                   theme::kText, typography::kPrimary);
  display.drawText("准备好后按 A", margin(display),
                   layout::kPrepareSecondaryY, theme::kMuted,
                   typography::kSecondary);
  drawFooter(display, "A 开始起卦");
}

void renderCasting(
    hardware::Display& display,
    const application::DivinationSession& session) noexcept {
  clearScreen(display);
  char heading[24]{};
  std::snprintf(heading, sizeof(heading), "第 %u 爻",
                static_cast<unsigned>(session.lineCount() + 1));
  drawTitle(display, heading);
  drawCoinPrompt(display);
  display.drawText("摇一摇", margin(display), layout::kCastingActionY,
                   theme::kText, typography::kPrimary);
  drawFooter(display, "A 也可以");
}

void renderLineResult(
    hardware::Display& display,
    const application::DivinationSession& session,
    const CoinAnimation* animation) noexcept {
  clearScreen(display);
  const auto* line = session.latestLine();
  if (line == nullptr) {
    drawTitle(display, "一爻");
    display.drawText("尚未起卦", margin(display), layout::kContentY,
                     theme::kMuted, typography::kPrimary);
    drawFooter(display, "A 返回");
    return;
  }

  drawLineHeading(display, line->position);
  if (animation != nullptr && animation->isActive()) {
    drawCoinAnimation(display, *animation, layout::kLineAnimationY);
    display.drawText("翻转中…", margin(display),
                     layout::kLineAnimationStatusY, theme::kMuted,
                     typography::kSecondary);
    drawFooter(display, "请稍候");
    return;
  }

  drawCoinResult(display, line->coins);
  display.drawText(domain::yinYangName(line->yinYang), margin(display),
                   layout::kLineResultValueY, theme::kText,
                   typography::kPrimary);
  display.drawText(line->moving ? "动爻" : "静爻", margin(display),
                   layout::kLineResultMovingY, theme::kAccent,
                   typography::kPrimary);
  drawFooter(display, session.isComplete() ? "A 查看本卦" : "A 继续");
}

void renderHexagramResult(
    hardware::Display& display,
    const application::DivinationSession& session) noexcept {
  const auto& result = session.result();
  if (!result.has_value()) {
    clearScreen(display);
    drawTitle(display, "本卦");
    display.drawText("结果不可用", margin(display), layout::kContentY,
                     theme::kMuted, typography::kPrimary);
    drawFooter(display, "A 返回");
    return;
  }

  const int lineTop = drawResultHeader(display, "本卦", result->original);
  drawMovingSummary(display, *result, layout::kResultMovingY);
  drawHexagram(display, result->original, lineTop);
  drawFooter(display, result->hasMovingLines() ? "A 查看之卦" : "A 重新起卦");
}

void renderTransformedResult(
    hardware::Display& display,
    const application::DivinationSession& session) noexcept {
  const auto& result = session.result();
  if (!result.has_value()) {
    clearScreen(display);
    drawTitle(display, "之卦");
    display.drawText("暂无结果", margin(display), layout::kContentY,
                     theme::kMuted, typography::kPrimary);
    drawFooter(display, "A 重新起卦");
    return;
  }
  if (!result->transformed.has_value()) {
    // The state machine does not enter this screen without moving lines.  If
    // a stale caller does, keep the user on the useful original result
    // instead of rendering an empty changed-hexagram page.
    renderHexagramResult(display, session);
    return;
  }

  const int lineTop = drawResultHeader(display, "→ 之卦", *result->transformed);
  drawHexagram(display, *result->transformed, lineTop);
  drawFooter(display, "A 重新起卦");
}

void renderResetConfirm(hardware::Display& display) noexcept {
  clearScreen(display);
  drawTitle(display, "重新起卦");
  display.drawText("重新开始？", margin(display), layout::kResetQuestionY,
                   theme::kText, typography::kPrimary);
  drawFooter(display, "A 确认  B 返回");
}

void renderWifiSettings(hardware::Display& display,
                        const application::WifiController& wifi) noexcept {
  clearScreen(display);
  drawTitle(display, "Wi-Fi");
  const bool connected = wifi.state() == application::WifiState::Connected;
  display.drawText(connected ? "已连接" : "未连接", margin(display),
                   layout::kWifiStatusY, theme::kText, typography::kPrimary);
  if (connected) {
    drawSsid(display, wifi);
  }
  drawFooter(display, "A 连接  B 返回");
}

void renderWifiConnecting(hardware::Display& display) noexcept {
  clearScreen(display);
  drawTitle(display, "Wi-Fi");
  display.drawText("正在连接…", margin(display), layout::kWifiStatusY,
                   theme::kText, typography::kPrimary);
  drawFooter(display, "B 取消");
}

void renderWifiConnected(hardware::Display& display,
                         const application::WifiController& wifi) noexcept {
  clearScreen(display);
  drawTitle(display, "Wi-Fi");
  display.drawText("已连接", margin(display), layout::kWifiStatusY,
                   theme::kText, typography::kPrimary);
  drawSsid(display, wifi);
  drawFooter(display, "A 断开");
}

void renderWifiFailed(hardware::Display& display,
                      const application::WifiController& wifi) noexcept {
  (void)wifi;
  clearScreen(display);
  drawTitle(display, "Wi-Fi");
  display.drawText("连接失败", margin(display), layout::kWifiStatusY,
                   theme::kText, typography::kPrimary);
  drawFooter(display, "A 重试  B 返回");
}

void renderWifiTimeout(hardware::Display& display) noexcept {
  clearScreen(display);
  drawTitle(display, "Wi-Fi");
  display.drawText("连接失败", margin(display), layout::kWifiStatusY,
                   theme::kText, typography::kPrimary);
  drawFooter(display, "A 重试  B 返回");
}

}  // namespace jinggua::ui
