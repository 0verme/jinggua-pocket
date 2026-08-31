# StickS3 Hardware Notes

## 已确认的 target

本项目只支持 M5Stack StickS3 / ESP32-S3。M5Stack 官方 StickS3 文档的
PlatformIO 示例明确给出：

```ini
[env:m5stack-sticks3]
platform = espressif32@6.12.0
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.partitions = default_8MB.csv
board_build.arduino.memory_type = qio_opi
```

仓库的 `platformio.ini` 按该依据配置为唯一硬件 environment，没有加入
StickC、StickC Plus、StickC Plus2 或其他板卡。

StickS3 官方规格/PinMap 页面确认：

- SoC 为 ESP32-S3-PICO-1-N8R8，8 MB Flash、8 MB Octal PSRAM。
- 内置显示驱动为 ST7789P3。
- IMU 为 BMI270，地址 `0x68`，I2C 使用 G48/G47。
- KEY1/KEY2 对应 ESP32-S3 G11/G12；M5Unified 将 KEY1 映射为 `M5.BtnA`、KEY2 映射为 `M5.BtnB`。
- 官方 Arduino 文档说明按钮使用 M5Unified `Button_Class`，IMU 使用
  M5Unified `IMU_Class`，显示使用 M5GFX。

## Library 选择

初始依赖固定为官方仓库 tag：

```ini
M5Unified=https://github.com/m5stack/M5Unified.git#0.2.21
M5PM1=https://github.com/m5stack/M5PM1.git#1.0.7
```

`M5Unified` 的版本与 StickS3 文档要求的 `>=0.2.12` 对齐；`M5PM1` 是
StickS3 官方 PlatformIO 示例列出的电源管理依赖。版本固定用于减少后续
bring-up 时的漂移；升级必须单独验证。

参考：

- <https://docs.m5stack.com/en/core/StickS3>
- <https://docs.m5stack.com/en/arduino/m5sticks3/button>
- <https://docs.m5stack.com/en/arduino/m5sticks3/imu>
- <https://docs.m5stack.com/en/arduino/m5sticks3/display>
- <https://github.com/m5stack/M5Unified>
- <https://github.com/m5stack/M5PM1>

## 代码边界

`M5.begin()` 在 `hardware::Display::begin()` 中调用；之后：

- `StickS3Buttons::poll()` 调用 `M5.update()`，使用 `M5.BtnA.wasClicked()`、
  `M5.BtnB.wasClicked()` 和 hold API；初始化时显式设置
  M5Unified 的 10 ms debounce，不在业务层读 G11/G12。
- `StickS3Imu::read()` 使用 `M5.Imu.update()` 与 `getImuData()`，只输出
  `ImuSample`。
- `Display` 使用 `M5.Display` 的 `fillScreen`、`drawString` 和 `drawLine`。
- `Esp32RandomProvider` 使用 `esp_random()`，不让 `random()` 散落在业务代码。
- `Esp32WifiManager` 实现 `application::WifiController` 接口：
  `WiFi.begin()` 非阻塞、`update(nowMs)` 轮询推进、15 秒超时；
  凭据来自编译期宏 `JINGGUA_WIFI_SSID` / `JINGGUA_WIFI_PASSWORD`，
  通过 `tools/wifi_credentials.py` extra_script 从环境变量注入。

按钮的电气细节和 IMU 初始化交给官方库，避免重复猜测极性、寄存器或 I2C
初始化顺序。

## IMU 摇动检测

`ShakeDetector` 的输入为三轴加速度、三轴角速度和毫秒时间戳。固件默认启用，
当前逻辑：

1. 加速度 magnitude 或角速度 magnitude 超过各自门限时记录一个峰值。
2. 峰值必须先回到释放门限以下，并满足最小峰间隔。
3. 在 detection window 内达到所需峰值数才产生一次 SHAKE。
4. 触发后进入 cooldown，避免一次摇动生成多个爻。

默认值在 `hardware/imu.h` 中，并作为可审查的校准起点：

- magnitude threshold：`1.6g`
- angular velocity threshold：`180 dps`
- detection window：`500ms`
- minimum peak interval：`80ms`
- cooldown：`1200ms`
- required peaks：`2`

这些仍需在真实 StickS3 上观察静止、轻拿、一次摇动和连续摇动数据后继续
校准。Shake 只在 Casting 和未完成的 LineResult 状态被采纳；完成六爻后
检测器被 reset，不会再生成额外爻。Button 始终可作为 fallback，且两种
输入都调用同一个 `DivinationSession::castLine()`。

## 真机安全边界

Phase 0 不改变电源管理、不驱动扬声器、不联网，也不写入用户问题。第一次
真机 Bring-up 请按 README 的 Next Step 先验证刷机、屏幕、按钮和原始 IMU
数据，再开启更强的 shake 行为。

## Hardware v0.1 Bring-up validation

Tested hardware: M5Stack StickS3
MCU: ESP32-S3-PICO-1-N8R8
Date: 2026-08-30
PlatformIO environment: m5stack-sticks3
Firmware source: feat/hardware-v0.1-bringup (bring-up diagnostics enabled)
Firmware commit: 2ea2845

- USB upload: PASS (ESP32-S3-PICO-1, 8MB flash, 8MB PSRAM)
- USB serial: PASS (Windows dynamically assigned COM port; COM3 during this session)
- Display initialization: PASS; detected size 135x240 on the previous bring-up firmware; the typography branch keeps the same display path and loads the generated Noto Sans SC Medium VLW subset
- Display rotation/readability: the previous bring-up verified the panel path with `efontCN_10`; this branch adds the 240-glyph anti-aliased subset. Asset/build checks pass, while visual confirmation of the new font remains required per device session
- BtnA: PASS; BtnA -> PrimaryClick, M5.update() active
- BtnB: NOT VERIFIED; reopened `COM3 @ 115200` monitor 后再次短按未捕获 BtnB 事件。M5Stack 官方资料与 M5Unified `0.2.21` 实现均确认 `BtnA=KEY1/GPIO11`、`BtnB=KEY2/GPIO12`，当前代码映射一致；因此暂无代码 root cause，仍需确认实际物理按键/电气路径
- BMI270: PASS; accelerometer and gyroscope samples observed
- Button full flow: PASS; six lines produced in order, index 0 = 初爻, index 5 = 上爻
- Shake Detector: CODE VERIFIED; 默认启用，native tests 覆盖阈值、释放、峰间隔和 cooldown；仍需新固件真机回归

First captured button-mode result:

- Lines bottom-to-top: 少阳, 少阴, 老阳, 少阴, 少阴, 少阳
- Original hexagram: 山火贲 (22)
- Moving line: 三爻
- Transformed hexagram: 艮为山 (52)

此前旧版实验逻辑的一次交互曾观察到 8 次 `TRIGGERED`。本 Issue 已加入
释放门限、最小峰间隔、cooldown，以及完成态 reset；新固件仍应在真机上复测
正常拿起、明显摇动、单次摇动和连续六次摇动。

### Bring-up matrix

| 项目 | 状态 |
| --- | --- |
| USB / Flash | PASS |
| Display | PASS |
| BtnA | PASS |
| BtnB | NOT VERIFIED — monitor capture had no event; authoritative mapping matches code, physical input path remains the unresolved root cause |
| BMI270 | PASS |
| Button Mode | PASS |
| Shake Detector | CODE VERIFIED — native tests pass; hardware regression pending |
| Shake Full Flow | CODE VERIFIED — six Shake events complete six lines; hardware regression pending |
| Transient I2C log | NON-BLOCKING / MONITOR |

Known bring-up note: M5Unified emitted transient I2C `ack wait` diagnostics during startup; display and BMI270 subsequently initialized and operated successfully. This remains NON-BLOCKING / MONITOR; no random delay, I2C clock, or initialization-order change was made.

The authoritative button mapping used for this verification is M5Stack StickS3 documentation plus the pinned M5Unified `0.2.21` implementation: `BtnA` reads KEY1/GPIO11 and `BtnB` reads KEY2/GPIO12.

Windows assigns the USB serial COM port dynamically. The project does not require a fixed upload_port or monitor_port.

## Wi-Fi（Issue #7 新增）

`Esp32WifiManager` 是对 ESP32 Arduino `WiFi` 的适配器，实现
`application::WifiController` 接口：

- 默认 `Off`，只有用户从设置页显式触发才 `WiFi.begin()`。
- 连接由主循环 `update(nowMs)` 非阻塞推进，不 `delay` 等待；
  `kConnectionTimeoutMs = 15000` 超时后停止尝试，等待用户重试/关闭。
- `Connected` / `Timeout` 后不自动重连，满足保守策略。
- 凭据来自编译期宏 `JINGGUA_WIFI_SSID` / `JINGGUA_WIFI_PASSWORD`，
  由 `tools/wifi_credentials.py` 从环境变量注入；未配置时固件仍可
  编译/离线运行，设置页提示「未配置 Wi-Fi」。
- 日志使用 `[WiFi]` 前缀，不打印密码；UI 只显示 SSID。

### Wi-Fi Provisioning TODO

本 Issue 只完成连接流程，未实现正式的凭据下发。未来候选方案（另行评估）：

- BLE / Serial 配网向导（设备进入 AP 模式 + App 下发 SSID/密码）
- 二维码配网（SmartConfig / WPA2-PSK 解析）
- WebServer 配网页
- 凭据持久化到 NVS

在实现 provisioning 之前，真机 Wi-Fi 验证使用构建期环境变量（见 README
「Wi-Fi 开发配置」），不会把真实 SSID/密码提交到仓库。
