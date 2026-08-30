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
- KEY1/KEY2 对应 ESP32-S3 G11/G12。
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
  `M5.BtnB.wasClicked()` 和 hold API，不在业务层读 G11/G12。
- `StickS3Imu::read()` 使用 `M5.Imu.update()` 与 `getImuData()`，只输出
  `ImuSample`。
- `Display` 使用 `M5.Display` 的 `fillScreen`、`drawString` 和 `drawLine`。
- `Esp32RandomProvider` 使用 `esp_random()`，不让 `random()` 散落在业务代码。

按钮的电气细节和 IMU 初始化交给官方库，避免重复猜测极性、寄存器或 I2C
初始化顺序。

## IMU 摇动检测（预留）

`ShakeDetector` 的输入为三轴加速度、三轴角速度和毫秒时间戳。当前逻辑：

1. 加速度 magnitude 超过阈值时记录 threshold crossing。
2. 在 detection window 内达到所需 crossing 数才产生一次 SHAKE。
3. 触发后进入 cooldown，避免一次摇动生成多个爻。

默认值在 `hardware/imu.h` 中，并明确标记为 provisional：

- magnitude threshold：`1.6g`
- detection window：`500ms`
- cooldown：`1200ms`
- required peaks：`2`

这些不是最终产品参数，必须在真实 StickS3 上观察静止、轻拿、一次摇动和
连续摇动数据后校准。Button Mode 不依赖 IMU，因此 IMU 不会阻塞 v0.1。

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

- USB upload: PASS (ESP32-S3-PICO-1, 8MB flash, 8MB PSRAM)
- USB serial: PASS (Windows dynamically assigned COM port; COM3 during this session)
- Display initialization: PASS; detected size 135x240
- Display rotation/readability: CODE VERIFIED; visual confirmation required per device session
- BtnA: PASS; BtnA -> PrimaryClick, M5.update() active
- BtnB: NOT VERIFIED in the captured full-flow log
- BMI270: PASS; accelerometer and gyroscope samples observed
- Button full flow: PASS; six lines produced in order, index 0 = 初爻, index 5 = 上爻
- ShakeDetector: EXPERIMENTAL; real triggers observed, but single-shake calibration was not completed and repeated triggers were observed during the trial

First captured button-mode result:

- Lines bottom-to-top: 少阳, 少阴, 老阳, 少阴, 少阴, 少阳
- Original hexagram: 山火贲 (22)
- Moving line: 三爻
- Transformed hexagram: 艮为山 (52)

Shake trial: 8 `TRIGGERED` events were observed during one interactive session; one event in CASTING produced line 1, while additional events were ignored outside the casting state. The current provisional detector requires further calibration.

Known bring-up note: M5Unified emitted transient I2C ack wait diagnostics during startup; display and BMI270 subsequently initialized and operated successfully.

Windows assigns the USB serial COM port dynamically. The project does not require a fixed upload_port or monitor_port.
