# 静卦掌中版 · JingGua Pocket

> 把一卦，握在手里。

JingGua Pocket is a pocket-sized I Ching divination device built for
**M5Stack StickS3 / ESP32-S3**. It is the hardware companion direction of the
[静卦 JingGua Web project](https://github.com/0verme/jinggua), not a copy of
another divination firmware.

## 项目简介

长期愿景是让设备完成实体摇卦，在本地生成六爻、本卦、动爻与之卦，再
由用户明确触发 Wi-Fi 联动，把完整解卦、历史与分享交给 JingGua Web。

当前仓库只做 **Phase 0 工程初始化 + Hardware v0.2 离线骨架**：先把领域
逻辑、输入抽象、状态机和真机硬件边界建立清楚，不提前引入云端、AI 或
用户数据收集。

## 当前状态

- PlatformIO + Arduino Framework 工程已固定到 `m5stack-sticks3`。
- Button Mode 已实现：每次按键生成一爻，严格从初爻到上爻。
- 三枚铜钱、六爻、本卦、动爻、之卦和 64 卦基础映射已实现。
- IMU Shake Mode 已启用：`ShakeDetector` 使用加速度/角速度阈值、释放门限、
  峰间隔与 cooldown；Button 仍可作为 fallback。
- **Wi-Fi 流程已实现（Issue #7，离线优先）**：设备默认关闭射频，只有用户
  从设置页显式触发才连接；连接异步非阻塞、15 秒超时、失败/超时不自动
  重连。当前只完成连接流程，不包含 JingGua API、设备绑定、二维码或 OTA。
  凭据通过构建期环境变量注入，不提交到仓库（见下文「Wi-Fi 开发配置」）。
- 麦克风 Research 固件已提供独立的 `m5stack-sticks3-mic-research` 环境；
  该环境只做显式触发的短时 PCM 统计，不属于产品语音流程。详见
  [`docs/microphone-research.md`](docs/microphone-research.md)。
- UI 是低饱和、黑底、米白与铜色的基础骨架，使用 Noto Sans SC Medium
  的 12 px 中文子集，暂不包含复杂动画或完整《周易》文本。
- v0.2 完全离线运行起卦，Wi-Fi 只在用户明确动作后开启。

## Hardware

| 项目 | 选择 |
| --- | --- |
| Target | M5Stack StickS3 |
| MCU | ESP32-S3-PICO-1-N8R8 / ESP32-S3 |
| PlatformIO board id | `esp32-s3-devkitc-1` |
| Framework | Arduino |
| Display driver | M5Unified / M5GFX，StickS3 内置 ST7789P3 |
| IMU | BMI270，通过 M5Unified `IMU_Class` |
| Button API | M5Unified `M5.BtnA` / `M5.BtnB` |

PlatformIO 的 board id、PSRAM/partition 设置和 StickS3 外设依据记录在
[`docs/hardware.md`](docs/hardware.md)，不在代码中猜测裸 GPIO 或 IMU 型号。

官方资料：

- [StickS3 product documentation](https://docs.m5stack.com/en/core/StickS3)
- [StickS3 Arduino Button](https://docs.m5stack.com/en/arduino/m5sticks3/button)
- [StickS3 Arduino IMU](https://docs.m5stack.com/en/arduino/m5sticks3/imu)
- [StickS3 Arduino Display](https://docs.m5stack.com/en/arduino/m5sticks3/display)
- [M5Unified](https://github.com/m5stack/M5Unified)
- [M5PM1](https://github.com/m5stack/M5PM1)

## Features

### Hardware v0.1

- 启动页：`静卦 / 安静地问一问`
- 准备页：默念所问之事
- Button Mode：按键触发 6 次起卦
- 每次显示三枚铜钱、爻类型和进度
- 结果页：本卦名称、卦序、上卦/下卦基础数据、动爻和之卦
- `InputEvent` 预留 `SHAKE`，硬件摇动检测不会侵入领域层

### 明确不在本阶段

AI 解卦、完整经文、**Wi-Fi API 交互（连接流程已实现）**、OAuth、数据库、BLE、OTA、
二维码、语音、自定义 PCB、外壳和多硬件兼容都不在 Phase 0 scope 内。

## Architecture

```text
hardware (StickS3 / M5Unified)
        │  InputEvent + Display primitives
        ▼
application (DivinationSession + StateMachine)
        │  domain values
        ▼
domain (coin / yao / trigram / hexagram / transformation)
        ▲
        │ static lookup tables
data (8 trigrams + 64 hexagrams)
```

- `domain/` 只依赖 C++ 标准库，不依赖屏幕、按钮、M5Stack SDK 或 Arduino。
- `application/` 通过 `RandomProvider` 接口取得铜钱输入，并管理六次起卦。
- `hardware/` 只负责 M5Unified、ESP32 entropy、按钮、IMU 和显示适配。
- `ui/` 将状态和领域值绘制成克制的屏幕内容，不参与卦象计算。
- `data/` 只存八卦与六十四卦基础映射，不塞入大量卦辞文本。
- `tools/generate_font_assets.py` 从 Noto Sans SC 生成实际文案子集；
  `docs/typography.md` 记录字体选择、字号层级和 Flash/RAM 测量。

详见：

- [`docs/architecture.md`](docs/architecture.md)
- [`docs/divination-model.md`](docs/divination-model.md)
- [`docs/state-machine.md`](docs/state-machine.md)

## Development

### Host native tests

领域与应用层可在没有 StickS3 的主机环境编译：

```bash
cmake -S test/native -B build/native
cmake --build build/native
ctest --test-dir build/native --output-on-failure
```

测试覆盖铜钱总数、四种爻、下上卦和六十四卦映射、动爻变换、起卦顺序
以及完整状态流。Host tests 使用 deterministic `RandomProvider`，不会调用
Arduino `random()`。

字体子集完整性检查：

```bash
python tools/check_font_asset.py
```

### StickS3 firmware

安装 PlatformIO 后执行：

```bash
pio run -e m5stack-sticks3
pio run -e m5stack-sticks3 -t upload
pio device monitor -b 115200
```

工程只有一个硬件 environment；native tests 使用独立 CMake harness，避免
为了主机测试伪造一个额外的硬件兼容 environment。

### Wi-Fi 开发配置

固件默认不定义任何 Wi-Fi 凭据；未配置时设置页点「连接」会提示「未配置
Wi-Fi」，固件仍可编译、刷机与离线起卦。需要真机验证 Wi-Fi 时，在构建前
设置环境变量（不要写进仓库）：

```bash
# macOS / Linux
JINGGUA_WIFI_SSID="你的SSID" JINGGUA_WIFI_PASSWORD="你的密码" pio run -e m5stack-sticks3

# Windows (PowerShell)
$env:JINGGUA_WIFI_SSID="你的SSID"
$env:JINGGUA_WIFI_PASSWORD="你的密码"
pio run -e m5stack-sticks3
```

`tools/wifi_credentials.py` 会把这些值以 C 字符串字面量注入 `build_flags`；
`.gitignore` 已忽略 `.env` / `.env.*`，凭据不会进入提交。未来正式的
credential provisioning（例如 BLE/App 配网、WPS）不在本 Issue 范围，
详见 [`docs/hardware.md`](docs/hardware.md) 的「Wi-Fi Provisioning TODO」。

## Roadmap

详见 [`docs/roadmap.md`](docs/roadmap.md)。

- **v0.1**：StickS3、Button 起卦、三枚铜钱、六爻、本卦、动爻、之卦、离线。
- **v0.2**：IMU Shake、铜钱动画、音效、UI polish。
- **v0.3**：用户明确触发 Wi-Fi、JingGua API、设备绑定、二维码、手机完整解卦。
- **v0.4**：历史记录与 Web / Device 同步。
- **v0.5**：麦克风与语音问事。
- **v1.0**：定制外壳、稳定固件、OTA、正式 Release。

## Related Project

- [JingGua Web](https://github.com/0verme/jinggua)

## Privacy and disclaimer

v0.1 是离线工具，不上传用户所问之事，不做遥测、Analytics、设备指纹或
自动联网。v0.2 新增 Wi-Fi 连接流程，但**设备默认不联网**：射频在开机时
处于关闭状态，只有用户从设置页按 A 显式触发后才开始连接；连接成功后也不
做自动数据上报。未来联网能力必须由用户明确动作触发。

本项目是文化与自我反思用途的工具，不替代医疗、法律、财务或其他专业
判断。卦象算法和显示顺序会在测试与真机 Bring-up 中持续校验。

## License

License: **TBD**
