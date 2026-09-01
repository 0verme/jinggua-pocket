# StickS3 字体与排版

StickS3 的目标画布是 **135×240 portrait**。Issue #4 的字体方案继续作为
Pocket 的唯一中文字体：Noto Sans SC 可变字体的 **Medium（500）** 字重，以
12 px 栅格化为 M5GFX VLW 格式。字号放大仍由同一个字体实例处理，避免不同
页面出现字体风格漂移。

| 候选 | 取舍 | 决定 |
| --- | --- | --- |
| Noto Sans SC Regular（400） | 笔画更轻，12 px 下对低对比度面板余量较小 | 不选 |
| Noto Sans SC Medium（500） | 笔画更稳，仍保持无衬线字形和清晰字腔 | **选用** |
| 1-bit bitmap | Flash/RAM 更低，但灰阶边缘和小字号细节较弱 | 不作为主方案 |
| VLW 8-bit anti-aliased | 每个像素保留灰度，资源仍可按字形子集控制 | **选用** |

Noto CJK 的上游项目与授权见 [notofonts/noto-cjk](https://github.com/notofonts/noto-cjk)。

## 子集边界

`tools/generate_font_assets.py` 扫描 `src/` 与 `include/` 的固件文案，加入
ASCII `U+0020..U+007E`，再生成升序 Unicode 表。当前提交的资源为
**271 个字形、38,085 字节**，覆盖固定文案、三枚铜钱符号、标点、八卦名称和
六十四卦名称。新增页面文案后必须重新生成资源并运行检查；不提交完整中文字库，
也不依赖设备文件系统。

生成示例（Windows）：

```powershell
& D:\miniconda3\python.exe tools/generate_font_assets.py `
  --font C:\Windows\Fonts\NotoSansSC-VF.ttf
& D:\miniconda3\python.exe tools/check_font_asset.py
```

源字体文件不进入仓库；Noto Sans SC 按 SIL Open Font License 1.1 发布，生成物
只包含本项目实际需要的栅格字形。若系统字体路径不同，可通过 `--font` 指定
同版本的 Noto Sans SC 文件。

## 统一字号与行高

`include/jinggua/ui/typography.h` 只定义语义角色，页面不得直接散落
`setTextSize` 数字：

| 层级 | 常量 | `setTextSize` | 参考行高 |
| --- | --- | ---: | ---: |
| 主标题 | `kTitle` | 3 | 48 px |
| 分区标题 | `kSection` | 2 | 32 px |
| 主信息/主动作 | `kPrimary` | 2 | 32 px |
| 次要信息 | `kSecondary` | 1 | 16 px |
| 状态信息 | `kStatus` | 1 | 16 px |
| 页脚 | `kFooter` | 1 | 16 px |
| 铜钱面 | `kCoin` | 2 | 32 px |

坐标、safe area、页眉/页脚、铜钱间距和六爻行距统一由
`include/jinggua/ui/layout.h` 管理；颜色由 `include/jinggua/ui/theme.h` 管理。
详见 [`ui-guidelines.md`](ui-guidelines.md)。

## Flash / RAM 记录

这是资源级测量，不把编译器、链接器或 M5GFX 其他代码的波动误算成字体成本：

| 项目 | 当前方案 | 原 `efontCN_10` | 差值 |
| --- | ---: | ---: | ---: |
| Flash 字体 payload | 38,085 B | 158,417 B | **-120,332 B（约 -75.9%）** |
| 运行时字形表（PSRAM 优先） | 2,439 B | 由 M5GFX 内置静态表承担 | 新增约 2.4 KiB |

运行时表按 M5GFX `VLWfont::loadFont` 的布局计算：每个字形需要 4 字节 bitmap
偏移、2 字节 Unicode、1 字节宽度、1 字节步进和 1 字节横向偏移，即
`271 × 9 = 2,439 B`。字形 bitmap 按需读到栈上，不常驻 RAM；分配失败时会
回退到内置 Latin `Font0`，且不会把完整中文库重新链接进固件。

`check_font_asset.py` 会在 CI/本地检查源码中出现的每个字符都存在于已提交字体表，
避免真机才发现缺字，并持续保护六十四卦名称的完整覆盖。
