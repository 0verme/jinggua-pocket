#include "jinggua/ui/screens.h"

#include <cstddef>
#include <cstdio>

namespace jinggua::ui {
namespace {

constexpr hardware::Color kBlack = 0x0000;
constexpr hardware::Color kIvory = 0xFFDF;
constexpr hardware::Color kCopper = 0xB9A0;
constexpr hardware::Color kMuted = 0x8410;
constexpr int kMargin = 12;

void clearScreen(hardware::Display& display) noexcept {
  display.clear(kBlack);
}

void drawTitle(hardware::Display& display, const char* title) noexcept {
  display.drawText(title, kMargin, 8, kCopper, 2);
}

void drawFooter(hardware::Display& display, const char* footer) noexcept {
  display.drawText(footer, kMargin, display.height() - 20, kMuted, 1);
}

const char* coinSymbol(domain::CoinSide side) noexcept {
  return side == domain::CoinSide::Front ? "●" : "○";
}

void drawHexagram(hardware::Display& display,
                 const domain::Hexagram& hexagram, int top) noexcept {
  const int lineSpacing = (display.height() - top - 12) / 6;
  const int lineWidth = display.width() > 190 ? 150 : display.width() - 52;
  const int gap = lineWidth / 7;
  const int x = (display.width() - lineWidth) / 2;

  for (int screenIndex = 0; screenIndex < 6; ++screenIndex) {
    const std::size_t lineIndex = 5U - static_cast<std::size_t>(screenIndex);
    const int y = top + screenIndex * lineSpacing;
    if (hexagram.pattern[lineIndex] == domain::YinYang::Yang) {
      display.drawLine(x, y, x + lineWidth, y, kIvory);
    } else {
      const int segment = (lineWidth - gap) / 2;
      display.drawLine(x, y, x + segment, y, kIvory);
      display.drawLine(x + segment + gap, y, x + lineWidth, y, kIvory);
    }
    if (hexagram.lines[lineIndex].moving) {
      display.drawText("○", x + lineWidth + 5, y - 8, kCopper, 1);
    }
  }
}

int drawResultHeader(hardware::Display& display, const char* label,
                     const domain::Hexagram& hexagram) noexcept {
  clearScreen(display);
  drawTitle(display, label);
  display.drawText(hexagram.name, kMargin, 30, kIvory, 2);

  char number[24]{};
  std::snprintf(number, sizeof(number), "第 %u 卦", hexagram.number);
  display.drawText(number, kMargin, 52, kMuted, 1);

  if (display.height() >= 180) {
    char trigrams[48]{};
    std::snprintf(trigrams, sizeof(trigrams), "上%s · 下%s",
                  hexagram.upperTrigram.name, hexagram.lowerTrigram.name);
    display.drawText(trigrams, kMargin, 70, kMuted, 1);
    return 92;
  }

  char compact[64]{};
  std::snprintf(compact, sizeof(compact), "上%s 下%s",
                hexagram.upperTrigram.name, hexagram.lowerTrigram.name);
  display.drawText(compact, kMargin, 70, kMuted, 1);
  return 82;
}

}  // namespace

void renderWelcome(hardware::Display& display) noexcept {
  clearScreen(display);
  display.drawText("静卦", kMargin, 22, kCopper, 3);
  display.drawText("JINGGUA POCKET", kMargin, 64, kIvory, 1);
  display.drawText("安静地问一问", kMargin, 84, kIvory, 2);
  drawFooter(display, "A 开始");
}

void renderPrepare(hardware::Display& display) noexcept {
  clearScreen(display);
  display.drawText("静心", kMargin, 18, kCopper, 3);
  display.drawText("默念所问之事", kMargin, 64, kIvory, 2);
  display.drawText("准备好后，按 A 起卦", kMargin, 94, kMuted, 1);
  drawFooter(display, "离线 · 不记录所问");
}

void renderCasting(
    hardware::Display& display,
    const application::DivinationSession& session) noexcept {
  clearScreen(display);
  drawTitle(display, "起卦");
  char progress[24]{};
  std::snprintf(progress, sizeof(progress), "第 %u / 6 爻",
                static_cast<unsigned>(session.lineCount() + 1));
  display.drawText(progress, kMargin, 46, kIvory, 2);
  display.drawText("按 A 生成一爻", kMargin, 84, kMuted, 1);
  drawFooter(display, "从初爻开始 · 由下往上");
}

void renderLineResult(
    hardware::Display& display,
    const application::DivinationSession& session) noexcept {
  clearScreen(display);
  drawTitle(display, "一爻");
  const auto* line = session.latestLine();
  if (line == nullptr) {
    display.drawText("尚未起卦", kMargin, 54, kMuted, 1);
    return;
  }

  char progress[24]{};
  std::snprintf(progress, sizeof(progress), "第 %u 爻", line->position);
  display.drawText(progress, kMargin, 40, kIvory, 2);

  char coins[48]{};
  std::snprintf(coins, sizeof(coins), "%s %s %s",
                coinSymbol(line->coins.coins[0]),
                coinSymbol(line->coins.coins[1]),
                coinSymbol(line->coins.coins[2]));
  display.drawText(coins, kMargin, 72, kCopper, 2);

  char type[48]{};
  std::snprintf(type, sizeof(type), "%s · %u%s", domain::yaoTypeName(line->type),
                line->coins.total, line->moving ? " · 动" : "");
  display.drawText(type, kMargin, 104, kIvory, 1);
  drawFooter(display, session.isComplete() ? "A 查看本卦" : "A 继续");
}

void renderHexagramResult(
    hardware::Display& display,
    const application::DivinationSession& session) noexcept {
  const auto& result = session.result();
  if (!result.has_value()) {
    clearScreen(display);
    drawTitle(display, "本卦");
    display.drawText("结果不可用", kMargin, 54, kMuted, 1);
    return;
  }

  const int lineTop = drawResultHeader(display, "本卦", result->original);
  drawHexagram(display, result->original, lineTop);

  char moving[64]{};
  if (result->hasMovingLines()) {
    int offset = std::snprintf(moving, sizeof(moving), "动爻 ");
    for (std::uint8_t index = 0; index < result->movingCount; ++index) {
      offset += std::snprintf(moving + offset, sizeof(moving) - offset,
                              "%s%u", index == 0 ? "" : " · ",
                              result->movingPositions[index]);
    }
  } else {
    std::snprintf(moving, sizeof(moving), "无动爻");
  }
  display.drawText(moving, kMargin, display.height() - 28, kCopper, 1);
  drawFooter(display, result->hasMovingLines() ? "A 查看之卦" : "A 重新起卦");
}

void renderTransformedResult(
    hardware::Display& display,
    const application::DivinationSession& session) noexcept {
  const auto& result = session.result();
  if (!result.has_value() || !result->transformed.has_value()) {
    clearScreen(display);
    drawTitle(display, "之卦");
    display.drawText("没有之卦", kMargin, 54, kMuted, 1);
    return;
  }

  const int lineTop =
      drawResultHeader(display, "之卦", *result->transformed);
  drawHexagram(display, *result->transformed, lineTop);
  drawFooter(display, "A 重新起卦");
}

void renderResetConfirm(hardware::Display& display) noexcept {
  clearScreen(display);
  drawTitle(display, "重新起卦");
  display.drawText("要从头开始吗？", kMargin, 54, kIvory, 2);
  display.drawText("A 确认", kMargin, 88, kCopper, 1);
  display.drawText("B 返回结果", kMargin, 106, kMuted, 1);
}

}  // namespace jinggua::ui
