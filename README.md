# 静卦掌中版 · JingGua Pocket

> 把一卦，握在手里。

JingGua Pocket is a pocket-sized I Ching divination device built for
**M5Stack StickS3 / ESP32-S3**. It is the hardware companion direction of the
[静卦 JingGua Web project](https://github.com/0verme/jinggua), not a copy of
another divination firmware.

## 项目简介

长期愿景是让设备完成实体摇卦，在本地生成六爻、本卦、动爻与之卦，再
由用户明确触发 Wi-Fi 联动，把完整解卦、历史与分享交给 JingGua Web。

当前仓库包含 **Phase 0 工程初始化 + Hardware v0.2 离线骨架 + Issue #8
Connected JingGua 最小 API Contract**：先把领域逻辑、输入抽象、状态机和真机
硬件边界建立清楚；可选联网只上传已在本地完成的结果，不引入云端随机、AI 或
用户问题数据收集。

## 当前状态

- PlatformIO + Arduino Framework 工程已固定到 `m5stack-sticks3`。
- Button Mode 已实现：每次按键生成一爻，严格从初爻到上爻。
- 三枚铜钱、六爻、本卦、动爻、之卦和 64 卦基础映射已实现。
- IMU Shake Mode 已启用：`ShakeDetector` 使用加速度/角速度阈值、释放门限、
  峰间隔与 cooldown；Button 仍可作为 fallback。
- **Wi-Fi 与 JingGua API 已实现（Issue #7/#8，离线优先）**：设备启动时将
  radio 置为关闭，只有用户从设置页显式触发才连接；连接异步非阻塞、15 秒超时、
  失败/超时不自动重连。完整六爻先在本地生成，结果页按 B 才上传；上传使用
  HTTPS、CA 校验、bounded payload/response、错误分类和手动 retry。当前不包含
  设备绑定、二维码、历史自动同步或 OTA。凭据通过构建期环境变量注入，不提交到
  仓库（见下文「Wi-Fi/API 开发配置」）。
- **自动息屏与低功耗路径已实现（Issue #5）**：`Active → Dim → Display Off
  → Light Sleep → Wake`，Button wake 和 state-preserving architecture 已
  接入；真实电流、唤醒稳定性和续航仍为 `PENDING DEVICE VALIDATION`。
- **声音反馈已实现（Issue #3）**：使用短合成 tone 提供 Start、Cast、Complete
  和 Error 确认；固定单 channel、非阻塞、队列满时丢弃，Settings 长按可
  runtime mute。默认固件启用 Speaker，麦克风 research environment 保持
  Speaker 关闭。
- 麦克风 Research 固件已提供独立的 `m5stack-sticks3-mic-research` 环境；
  该环境只做显式触发的短时 PCM 统计，不属于产品语音流程。详见
  [`docs/microphone-research.md`](docs/microphone-research.md)。
- UI 已针对 StickS3 的 135×240 portrait 小屏完成基础 polish：一屏一个主要
  动作、少文字、大字号、强层级，使用统一 layout/theme/typography constants；
  Pocket 只展示本卦、动爻与之卦，不内置完整《周易》文本。
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
| Audio | M5Unified `M5.Speaker` / ES8311 I2S TX（普通固件） |

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

- 启动页：`静卦 / 摇一摇 / 或按 A`
- 准备页：默念所问之事
- Casting：显示当前爻序和三枚铜钱，Shake 为主动作，Button 为 fallback
- LineResult：CoinAnimation 结束后显示三枚铜钱、阴/阳、静爻/动爻和下一步
- 结果页：本卦名称、完整六爻图、动爻和之卦；无动爻时不进入空的之卦页
- `InputEvent` 预留 `SHAKE`，硬件摇动检测不会侵入领域层

### 明确不在本阶段

AI 解卦、完整经文、OAuth、数据库管理、BLE 配网、设备绑定、历史自动同步、
OTA、二维码、语音、自定义 PCB、外壳和多硬件兼容都不在本 Issue #8 scope 内。

## Architecture

```text
hardware (StickS3 / M5Unified / HTTPS)
        │  InputEvent + adapters
        ▼
application (Session + StateMachine + ApiClient)
        │  const domain values / ports
        ▼
domain (coin / yao / trigram / hexagram / transformation)
        ▲
        │ static lookup tables
data (8 trigrams + 64 hexagrams)
```

- `domain/` 只依赖 C++ 标准库，不依赖屏幕、按钮、M5Stack SDK 或 Arduino。
- `application/` 通过 `RandomProvider` 接口取得铜钱输入，并管理六次起卦。
- `hardware/` 只负责 M5Unified、ESP32 entropy、按钮、IMU、显示、电源和音频适配。
- `PowerManager` 是可在主机测试的 inactivity 状态逻辑；
  `StickS3PowerController` 将其映射到 Display brightness、Display Off 和
  ESP32-S3 light sleep。
- `ui/` 将状态和领域值绘制成克制的屏幕内容，不参与卦象计算；
  `ui/layout.h`、`ui/theme.h`、`ui/typography.h` 统一小屏布局、颜色和字号。
- `data/` 只存八卦与六十四卦基础映射，不塞入大量卦辞文本。
- `tools/generate_font_assets.py` 从 Noto Sans SC 生成实际文案子集；
  `docs/typography.md` 记录字体选择、字号层级和 Flash/RAM 测量。

详见：

- [`docs/architecture.md`](docs/architecture.md)
- [`docs/divination-model.md`](docs/divination-model.md)
- [`docs/state-machine.md`](docs/state-machine.md)
- [`docs/power-management.md`](docs/power-management.md)
- [`docs/coin-animation.md`](docs/coin-animation.md)
- [`docs/api-contract.md`](docs/api-contract.md)
- [`docs/audio-feedback.md`](docs/audio-feedback.md)
- [`docs/ui-guidelines.md`](docs/ui-guidelines.md)

## Development

### Host native tests

领域与应用层可在没有 StickS3 的主机环境编译：

```bash
cmake -S test/native -B build/native
cmake --build build/native
ctest --test-dir build/native --output-on-failure
```

测试覆盖铜钱总数、四种爻、下上卦和六十四卦映射、动爻变换、起卦顺序、
完整状态流、PowerManager 的 inactivity threshold、活动唤醒、sleep inhibition 和
`millis()` wrap-around，以及 API contract 的 fake transport/client、离线保护、
错误、bounded response 和手动 retry。Host tests 使用 deterministic
`RandomProvider`，不会调用 Arduino `random()`。

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

### Wi-Fi/API 开发配置

固件默认不定义任何 Wi-Fi 凭据或 API 配置；未配置时设置页点「连接」会提示
「未配置 Wi-Fi」，即使连接成功但没有 API URL/CA，上传也会安全失败。固件仍可
编译、刷机、离线起卦和保存本地历史。需要真机验证时，在构建前设置环境变量
（不要写进仓库）：

```bash
# macOS / Linux
JINGGUA_WIFI_SSID="你的SSID" JINGGUA_WIFI_PASSWORD="你的密码" pio run -e m5stack-sticks3

# Windows (PowerShell)
$env:JINGGUA_WIFI_SSID="你的SSID"
$env:JINGGUA_WIFI_PASSWORD="你的密码"
$env:JINGGUA_API_URL="https://your-jinggua-host.example/api/divinations"
$env:JINGGUA_API_ROOT_CA="-----BEGIN CERTIFICATE-----..."
pio run -e m5stack-sticks3
```

`tools/wifi_credentials.py` 注入 Wi-Fi 构建参数，`tools/api_config.py` 注入
API URL 和公开 CA certificate；脚本不会打印 API 配置。没有 CA 时不会调用
`setInsecure()`，而是 fail closed。
`.gitignore` 已忽略 `.env` / `.env.*`，凭据不会进入提交。未来正式的
credential provisioning（例如 BLE/App 配网、WPS）不在本 Issue 范围，
详见 [`docs/hardware.md`](docs/hardware.md) 的「Wi-Fi Provisioning TODO」。

## Roadmap

详见 [`docs/roadmap.md`](docs/roadmap.md)。

- **v0.1**：StickS3、Button 起卦、三枚铜钱、六爻、本卦、动爻、之卦、离线。
- **v0.2**：IMU Shake、铜钱动画、音效、135×240 UI polish（Issue #6）、自动息屏与
  低功耗管理（Issue #5）。
- **v0.3**：用户明确触发 Wi-Fi、JingGua API 最小 Contract（Issue #8）；设备绑定、
  二维码和手机完整解卦另行实现。
- **v0.4**：历史记录与 Web / Device 同步。
- **v0.5**：麦克风与语音问事。
- **v1.0**：定制外壳、稳定固件、OTA、正式 Release。

## Related Project

- [JingGua Web](https://github.com/0verme/jinggua)

## Privacy and disclaimer

设备不上传用户所问之事，不采集 MAC/芯片唯一 ID，不做自动联网或后台上报。
Issue #8 新增的 API 只在用户完成六爻并在结果页按 B 后发送已完成结果；设备默认
在开机时关闭 radio，连接成功后也不自动上传。请求不包含长期 credential；未来
正式设备绑定属于 #9。

本项目是文化与自我反思用途的工具，不替代医疗、法律、财务或其他专业
判断。卦象算法和显示顺序会在测试与真机 Bring-up 中持续校验。

## License

License: **TBD**
