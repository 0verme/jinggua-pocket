#include "jinggua/ui/history_screen.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "jinggua/data/hexagrams.h"
#include "jinggua/domain/history_record.h"
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

void drawMovingLines(hardware::Display& display,
                     const domain::HistoryRecord& record, int y) noexcept {
  if (record.movingMask == 0) {
    display.drawText("无动爻", margin(display), y, theme::kAccent,
                     typography::kStatus);
    return;
  }

  char moving[48]{};
  std::size_t offset = static_cast<std::size_t>(
      std::snprintf(moving, sizeof(moving), "动："));
  bool first = true;
  for (std::uint8_t line = 0; line < 6; ++line) {
    if ((record.movingMask & (1U << line)) == 0 || offset >= sizeof(moving)) {
      continue;
    }
    const int written = std::snprintf(
        moving + offset, sizeof(moving) - offset, "%s%s",
        first ? "" : "、", linePositionName(line + 1));
    if (written <= 0) {
      break;
    }
    offset += static_cast<std::size_t>(written);
    first = false;
  }
  display.drawText(moving, margin(display), y, theme::kAccent,
                   typography::kStatus);
}

}  // namespace

void renderHistory(hardware::Display& display,
                   const application::StateMachine& stateMachine) noexcept {
  clearScreen(display);
  display.drawText("历史", margin(display), layout::kHeaderY, theme::kAccent,
                   typography::kSection);

  const auto* history = stateMachine.history();
  if (history == nullptr || history->count() == 0) {
    display.drawText("暂无记录", margin(display), layout::kHistoryEmptyY,
                     theme::kText, typography::kPrimary);
    display.drawText("起卦后自动保存", margin(display),
                     layout::kHistoryEmptyHintY, theme::kMuted,
                     typography::kSecondary);
    drawFooter(display, "长按返回");
    return;
  }

  char position[32]{};
  std::snprintf(position, sizeof(position), "第 %u / %u 条",
                static_cast<unsigned>(stateMachine.historyCursor() + 1),
                static_cast<unsigned>(history->count()));
  display.drawText(position, margin(display), layout::kHistoryOrderY,
                   theme::kMuted, typography::kStatus);

  domain::HistoryRecord record;
  if (!history->get(stateMachine.historyCursor(), record)) {
    display.drawText("记录不可用", margin(display), layout::kHistoryEmptyY,
                     theme::kText, typography::kPrimary);
    drawFooter(display, "A 更早  B 更新", "长按返回");
    return;
  }

  const auto* original = data::findHexagramByNumber(record.originalNumber);
  const char* originalName = original != nullptr ? original->name : "未知卦";
  display.drawText("本卦", margin(display), layout::kHistoryOriginalLabelY,
                   theme::kMuted, typography::kStatus);
  display.drawText(originalName, margin(display), layout::kHistoryOriginalNameY,
                   theme::kText, typography::kPrimary);

  drawMovingLines(display, record, layout::kHistoryMovingY);

  if (record.changedNumber != 0) {
    const auto* changed = data::findHexagramByNumber(record.changedNumber);
    const char* changedName = changed != nullptr ? changed->name : "未知卦";
    display.drawText("→ 之卦", margin(display),
                     layout::kHistoryChangedLabelY, theme::kAccent,
                     typography::kStatus);
    display.drawText(changedName, margin(display),
                     layout::kHistoryChangedNameY, theme::kText,
                     typography::kPrimary);
  } else {
    display.drawText("无动爻", margin(display), layout::kHistoryChangedLabelY,
                     theme::kAccent, typography::kStatus);
  }

  drawFooter(display, "A 更早  B 更新", "长按返回");
}

}  // namespace jinggua::ui
