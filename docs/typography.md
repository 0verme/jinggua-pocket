# StickS3 字体与排版

Issue #4 的目标是让 135x240 的 StickS3 小屏在正常手持距离可读。当前方案
选择 Noto Sans SC 可变字体的 **Medium（500）** 字重，以 12 px 栅格化为
M5GFX VLW 格式。VLW 保留每个像素的 8-bit 灰度，面板绘制时由 M5GFX 做
抗锯齿混色；字号放大仍由同一个字体实例处理，避免不同页面出现字体风格
漂移。

| 候选 | 取舍 | 决定 |
| --- | --- | --- |
| Noto Sans SC Regular（400） | 笔画更轻，12 px 下对低对比度面板余量较小 | 不选 |
| Noto Sans SC Medium（500） | 笔画更稳，仍保持无衬线字形和清晰字腔 | **选用** |
| 1-bit bitmap | Flash/RAM 更低，但灰阶边缘和小字号细节较弱 | 不作为主方案 |
| VLW 8-bit anti-aliased | 每个像素保留灰度，资源仍可按字形子集控制 | **选用** |

Noto CJK 的上游项目与授权见 [notofonts/noto-cjk](https://github.com/notofonts/noto-cjk)。

## 子集边界

`tools/generate_font_assets.py` 扫描 `src/` 与 `include/` 的固件文案，加入
ASCII `U+0020..U+007E`，再生成升序 Unicode 表。当前结果为 **240 个字形、
33,326 字节**，覆盖固定文案、三枚铜钱符号、标点、八卦名称和六十四卦名称。
不提交完整中文字库，也不依赖设备文件系统。

生成示例（Windows）：

```powershell
& D:\miniconda3\python.exe tools\generate_font_assets.py `
  --font C:\Windows\Fonts\NotoSansSC-VF.ttf
& D:\miniconda3\python.exe tools\check_font_asset.py
```

源字体文件不进入仓库；Noto Sans SC 按 SIL Open Font License 1.1 发布，
生成物只包含本项目实际需要的栅格字形。若系统字体路径不同，可通过
`--font` 指定同版本的 Noto Sans SC 文件。

## Flash / RAM 记录

这是资源级测量，不把编译器、链接器或 M5GFX 其他代码的波动误算成字体成本：

| 项目 | 当前方案 | 原 `efontCN_10` | 差值 |
| --- | ---: | ---: | ---: |
| Flash 字体 payload | 33,326 B | 158,417 B | **-125,091 B（-79.0%）** |
| 运行时字形表（PSRAM 优先） | 2,160 B | 由 M5GFX 内置静态表承担 | 新增约 2.1 KiB |

运行时表按 M5GFX `VLWfont::loadFont` 的布局计算：每个字形需要 4 字节
bitmap 偏移、2 字节 Unicode、1 字节宽度、1 字节步进和 1 字节横向偏移，
即 `240 × 9 = 2,160 B`。字形 bitmap 按需读到栈上，不常驻 RAM；分配失败时
会回退到内置 Latin `Font0`，且不会把完整中文库重新链接进固件。

## 统一字号

页面只使用 `include/jinggua/ui/typography.h` 中的语义常量：

| 层级 | 常量 | `setTextSize` |
| --- | --- | ---: |
| 主标题 | `kTitle` | 3 |
| 分区标题 | `kSection` | 2 |
| 核心结果 | `kResult` | 2 |
| 正文 | `kBody` | 1 |
| 辅助信息 | `kAuxiliary` | 1 |
| 页脚 | `kFooter` | 1 |

固定提示使用短句（例如“按 A 生成一爻”），结果页保留完整卦名；
`check_font_asset.py` 会在 CI/本地检查源码中出现的每个字符都存在于已提交
字体表，避免真机才发现缺字。
