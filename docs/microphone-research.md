# StickS3 麦克风与设备端录音 Research

## 结论

> 本文是 PR #22 的早期 Research 记录。完整验收矩阵与当前 harness 说明见
> [`docs/research/microphone-validation.md`](research/microphone-validation.md)。

**PENDING DEVICE VALIDATION（不能用编译通过替代真机声学、时长和功耗证据）**。

StickS3 的硬件和 M5Unified 音频路径具备做短时、用户明确触发的本地采集的
条件：官方规格给出 ES8311 单声道音频编解码器、MEMS 麦克风（SNR 65 dB）和
8 MB PSRAM；当前锁定的 M5Unified `0.2.21` 也为 StickS3 配置了对应的 I2S
引脚并提供 `int16_t` 原始录音接口。

在 `M5Unified.cpp` 的 StickS3 分支中，I2S 映射为 MCLK=`GPIO18`、BCLK=`GPIO17`、
WS=`GPIO15`、DIN=`GPIO16`，使用 `I2S_NUM_1`；本仓库没有重新猜测或覆盖这些
引脚配置。

这个结论有明确边界：本次提交所在环境没有连接 StickS3，也没有 USB 电流计，
因此声学质量、可懂度和录音期间电流的验收项尚未取得真机证据。不能把规格或
编译成功写成“麦克风已经实测通过”。发布语音功能前必须完成下方的真机复测。

官方参考：

- [StickS3 产品规格](https://docs.m5stack.com/en/core/StickS3)
- [StickS3 Mic Arduino 示例](https://docs.m5stack.com/en/arduino/m5sticks3/mic)

## 已落实的 Research 固件

新增 PlatformIO 环境 `m5stack-sticks3-mic-research`，通过
`JINGGUA_ENABLE_MIC_RESEARCH=1` 显式开启。默认的
`m5stack-sticks3` 环境仍关闭麦克风，不改变 v0.1 离线起卦行为。

诊断固件启动后在串口打印配置，并接受单字节命令：

| 命令 | 场景 | 行为 |
| --- | --- | --- |
| `a` | ambient | 使用当前配置的环境噪声采集 |
| `v` | voice | 使用当前配置的近场说话采集 |
| `t` | transient | 使用当前配置的拍手/敲击等瞬态采集 |
| `r8000` / `r16000` | rate | 设置采样率（Hz） |
| `d5` / `d15` / `d30` | duration | 设置录音时长（秒） |

每次采集只在 RAM 中处理统计值，随后丢弃 PCM 样本；不会写 Flash、SD 卡，
不会联网，也不会上传用户语音。串口输出包含：

- 请求时长、实际时长、样本数和有效采样率；
- `PCM_S16LE`、单声道、块大小、块字节数和 DMA buffer 数；
- 最小/最大值、峰值、均值、RMS、削波计数和过零次数；
- 采集前后的 free heap / free PSRAM。

默认配置是 16,000 Hz、单声道、16-bit PCM、每块 256 samples、4 个 DMA
buffer、每次 5,000 ms。每块工作缓冲为 512 bytes；harness 支持用 `d5`、`d15`、
`d30` 选择 5/15/30 秒，刻意不分配整段长期 PCM 缓冲。完整的 buffer、heap、
queue 和 duration 说明见新的 validation 文档。

## 真机复测步骤

在 Windows 上从仓库根目录执行：

```powershell
python -m platformio run -e m5stack-sticks3-mic-research
python -m platformio run -e m5stack-sticks3-mic-research -t upload
python -m platformio device monitor -b 115200
```

串口出现 `[MicResearch] peripheral=stopped` 后，按 validation 文档设置
`r8000` / `r16000` 与 `d5` / `d15` / `d30`，各组合至少执行 5 次，并保存完整
串口日志（日志中只含统计值，不含语音样本）：

1. 设备静置，麦克风前方约 30 cm 无人说话，发送 `a`。
2. 保持相同距离，以固定音量朗读同一句中文，发送 `v`。
3. 在相同距离拍手一次，发送 `t`，观察峰值和削波。
4. 用 USB-C 电流计分别记录屏幕亮起但未采集的 idle 电流，以及发送 `v`
   期间的稳定电流；记录 5 次的中位数和增量。M5PM1 能读电压/供电源，不能
   代替 USB 电流计，因此不能从固件日志推导电流。

建议把每次结果整理为：

```text
case, elapsed_ms, sample_count, effective_rate_hz, rms_milli, peak,
clipped_samples, zero_crossings, free_heap_before, free_heap_after,
power_idle_ma, power_capture_ma
```

## 当前验收矩阵

| Issue #13 要求 | 当前证据 | 状态 |
| --- | --- | --- |
| 目标硬件麦克风输入 | 诊断环境和串口统计已实现；本机未连接 StickS3 | 待真机 |
| sample rate / buffer / RAM | 固件默认 16 kHz，可切换候选 rate，并打印 block/DMA/heap/PSRAM/queue 指标 | 部分完成 |
| 录音时长与编码 | 可配置 5/15/30 秒；M5Unified `int16_t` 原始接口，即 PCM S16LE 单声道 | 代码已验证，时序待真机 |
| 噪声、可懂度、典型限制 | 需要 `a` / `v` / `t` 五次日志和人工听感记录 | 待真机 |
| 录音期间功耗 | 需要外部 USB-C 电流计；当前环境没有仪表 | 待仪表 |
| 是否必须上传 | 当前实现不上传；只输出匿名统计值 | 已确认（当前范围） |
| 明确结论 | `PENDING DEVICE VALIDATION`：硬件路径有条件可行，但尚无声学/功耗真机证据 | 待真机 |

## 实现边界与替代方案

- 采集必须由用户明确触发，并设置硬时长上限；不实现后台持续监听。
- 设备端只适合短时原始采集和统计，不在 ESP32-S3 上实现语音识别或复杂编码。
- 若未来需要联网，先在设备端明确结束采集，再由用户授权传输；可在手机/Web
  侧把 PCM 转为 Opus/AAC 后上传，减少设备端 RAM 和功耗压力。
- 在声学与功耗实测完成前，不把 `v0.5` 的“麦克风与语音问事”标记为完成，
  也不引入 Wi-Fi、账号、历史记录或语音数据持久化。
