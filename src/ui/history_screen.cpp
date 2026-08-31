#include "jinggua/ui/history_screen.h"

#include <cstddef>
#include <cstdio>

#include "jinggua/data/hexagrams.h"
#include "jinggua/domain/history_record.h"
#include "jinggua/ui/typography.h"

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

const char* syncStatusLabel(domain::SyncStatus status) noexcept {
  switch (status) {
    case domain::SyncStatus::Pending:
      return "未同步";
    case domain::SyncStatus::Synced:
      return "已同步";
    case domain::SyncStatus::Failed:
      return "同步失败";
  }
  return "未知";
}

void drawMovingLines(hardware::Display& display,
                     const domain::HistoryRecord& record, int y) noexcept {
  if (record.movingMask == 0) {
    display.drawText("无动爻 · 本卦即变卦", kMargin, y, kMuted,
                     typography::kAuxiliary);
    return;
  }
  char moving[64]{};
  int offset = std::snprintf(moving, sizeof(moving), "动爻 ");
  bool first = true;
  for (std::uint8_t line = 0; line < 6; ++line) {
    if ((record.movingMask & (1U << line)) == 0) {
      continue;
    }
    offset += std::snprintf(moving + offset, sizeof(moving) - offset,
                            "%s%d", first ? "" : " · ", line + 1);
    first = false;
  }
  display.drawText(moving, kMargin, y, kCopper, typography::kAuxiliary);
}

}  // namespace

void renderHistory(hardware::Display& display,
                   const application::StateMachine& stateMachine) noexcept {
  clearScreen(display);
  display.drawText("历史", kMargin, 2, kCopper, typography::kSection);

  const auto* history = stateMachine.history();
  if (history == nullptr || history->count() == 0) {
    display.drawText("暂无记录", kMargin, 48, kIvory, typography::kResult);
    display.drawText("完成一次起卦后自动保存", kMargin, 88, kMuted,
                     typography::kAuxiliary);
    display.drawText("长按返回", kMargin, display.height() - 16, kMuted,
                     typography::kFooter);
    return;
  }

  char position[24]{};
  std::snprintf(position, sizeof(position), "%u / %u",
                static_cast<unsigned>(stateMachine.historyCursor() + 1),
                static_cast<unsigned>(history->count()));
  display.drawText(position, kMargin, 26, kIvory, typography::kAuxiliary);

  domain::HistoryRecord record;
  if (!history->get(stateMachine.historyCursor(), record)) {
    display.drawText("该记录已损坏", kMargin, 48, kMuted,
                     typography::kBody);
    return;
  }

  const auto* original = data::findHexagramByNumber(record.originalNumber);
  const char* originalName = original != nullptr ? original->name : "未知卦";
  char originalLine[40]{};
  std::snprintf(originalLine, sizeof(originalLine), "本卦 %s",
                originalName);
  display.drawText(originalLine, kMargin, 52, kIvory, typography::kResult);

  char meta[64]{};
  if (record.timestampValid && record.timestampEpoch != 0) {
    std::snprintf(meta, sizeof(meta), "记录 #%u · 时间 %u",
                  static_cast<unsigned>(record.localRecordId),
                  static_cast<unsigned>(record.timestampEpoch));
  } else {
    std::snprintf(meta, sizeof(meta), "记录 #%u · 离线时间",
                  static_cast<unsigned>(record.localRecordId));
  }
  display.drawText(meta, kMargin, 86, kMuted, typography::kAuxiliary);

  drawMovingLines(display, record, 104);

  if (record.changedNumber != 0) {
    const auto* changed = data::findHexagramByNumber(record.changedNumber);
    const char* changedName = changed != nullptr ? changed->name : "未知卦";
    char changedLine[40]{};
    std::snprintf(changedLine, sizeof(changedLine), "之卦 %s", changedName);
    display.drawText(changedLine, kMargin, 122, kIvory, typography::kResult);
  }

  display.drawText(syncStatusLabel(record.syncStatus), kMargin,
                   display.height() - 32, kMuted, typography::kAuxiliary);
  display.drawText("A 更早 B 更新 长按返回", kMargin, display.height() - 16,
                   kMuted, typography::kFooter);
}

}  // namespace jinggua::ui