# StickS3 小屏 UI 规范

Pocket 的目标画布是 **135×240** 的竖屏。它不是把手机网页缩小，而是让设备在
正常手持距离完成一次明确动作。

## 核心原则

- 一屏一个主要动作。
- 少文字、大字号、强层级。
- 尽量不滚动；结果页优先保证六爻图完整。
- Pocket 只展示本卦、动爻和之卦，不内置完整解卦文本。
- 技术状态不直接暴露给用户；显示自然产品语言。

## 统一布局

布局常量集中在 `include/jinggua/ui/layout.h`，颜色集中在
`include/jinggua/ui/theme.h`，字号和行高集中在
`include/jinggua/ui/typography.h`。页面不得重新定义 margin、footer 或铜钱
间距等坐标。

| 项目 | 规则 |
| --- | --- |
| 画布 | 135×240 portrait |
| 外边距 | 10 px；顶部安全区 6 px，底部安全区 8 px |
| 页眉 | y=6；分区标题使用 `kSection` |
| 页脚 | y=216；多行页脚先使用 y=196 |
| 主字号 | `kPrimary`，12 px Noto Sans SC 的 2 倍，16 px line box |
| 辅助/状态字号 | `kSecondary` / `kStatus`，12 px glyph，16 px line box |
| 铜钱中心 | x=31、67、103；统一 36 px 间距 |
| 六爻图 | y=116 起，15 px 行距，线段和动爻标记不越出安全区 |
| SSID | 单行显示，最多 8 个 Unicode 字符，过长以省略号收尾 |

`layout::isPocketLayoutSafe()` 和 native UI 测试锁定上述关键边界，避免新增
页面坐标越界、页脚覆盖正文或六爻图不完整。

## 页面动作

- Home：`静卦` → `摇一摇` / `或按 A`；B 进入设置。
- Prepare：默念所问，A 开始起卦。
- Casting：显示当前第几爻和三枚铜钱，摇动为主动作，A 为 fallback。
- LineResult：动画期间只等待；动画结束后显示铜钱、阴/阳、静爻/动爻和下一步。
- HexagramResult：本卦名称、完整六爻图、动爻摘要；有动爻时 A 查看之卦。
- TransformedResult：以 `→ 之卦` 标识变卦，只显示名称和完整六爻图。
- 无动爻：显示 `无动爻`，不进入空的之卦页面。
- History：只显示记录顺序、本卦、动爻和存在时的之卦；不显示 local id、sync
  status、schema、CRC 或原始 epoch。
- Wi-Fi：统一使用 `未连接`、`正在连接…`、`已连接`、`连接失败`，不显示错误码。

## 字体边界

继续复用 Noto Sans SC Medium subset。新增固件文案后必须重新生成
`font_data.cpp` / `font_data.h` 并运行 `tools/check_font_asset.py`；不能回退到
完整 `efontCN` 中文字库。六十四卦名称始终属于字体完整性检查范围。

## 真机检查清单

当前没有把本次改动标记为真机验收。获得 StickS3 后逐页检查：

- 中文在正常手持距离可读，不需凑近。
- Home 主动作一眼可见。
- CoinAnimation 帧率稳定且不遮挡标题。
- 六爻图六条线完整，动爻标记清楚。
- 本卦、动爻、之卦层级自然。
- Wi-Fi 和 History 页没有文字碰撞或截断。
- 所有页的页脚不覆盖正文。

状态：**PENDING DEVICE VISUAL VALIDATION**
