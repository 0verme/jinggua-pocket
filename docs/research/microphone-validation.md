# StickS3 Microphone Validation

## Status

**PENDING DEVICE VALIDATION**

本分支完成了硬件路径核对、可配置的离线 research harness、自动指标输出和存储估算；当前执行环境没有连接 StickS3，也没有 USB 电流计。因此不能把编译通过、芯片规格或 API 支持写成麦克风真机通过，更不能在没有音频样本的情况下判定可懂度。

本 Issue 是 Research，不实现 #14 Voice Question、Speech-to-Text、上传、Wi-Fi、云服务或后台监听。

## Hardware

目标板为 M5Stack StickS3，SoC 为 ESP32-S3-PICO-1-N8R8，规格为 8 MB Flash 和 8 MB Octal PSRAM。M5Stack 官方 StickS3 页面描述的音频路径是 ES8311 mono audio codec 加 high-sensitivity MEMS microphone；官方页面和当前 BSP 源码没有给出 MEMS capsule 的具体 part number，因此这里不猜测 microphone model。

当前工程锁定：

- `platform = espressif32@6.12.0`
- `framework = arduino`
- `M5Unified` `0.2.21`
- `M5PM1` `1.0.7`
- board definition：`esp32-s3-devkitc-1`，配合 StickS3 的 `qio_opi` / `default_8MB.csv`

M5Unified `0.2.21` 的 `M5Unified.cpp` 对 `board_M5StickS3` 设置：

| Signal | GPIO |
| --- | ---: |
| MCLK | 18 |
| BCLK | 17 |
| WS/LRCK | 15 |
| DIN/data in | 16 |
| I2S port | `I2S_NUM_1` |

这不是 PDM microphone path。因为 StickS3 同时提供 BCLK 和 WS，`Mic_Class::_setup_i2s()` 选择 I2S standard mode；PDM 分支只在没有 BCLK/WS 时启用。当前 harness 配置为 16-bit data、16-bit slot、mono、`stereo=false`，使用 M5Unified 的 `record(int16_t*, ...)` 接口。默认 input channel 是 right channel；没有启用 ADC、noise filter 或 software oversampling（`over_sampling=1`）。

证据来源：

- [M5Stack StickS3 official specification](https://docs.m5stack.com/en/core/StickS3)
- [M5Stack StickS3 microphone API/example](https://docs.m5stack.com/en/arduino/m5sticks3/mic)
- [M5Unified `Mic_Class`](https://github.com/m5stack/M5Unified/blob/master/src/utility/Mic_Class.hpp)
- 工程实际编译的 `.pio/libdeps/m5stack-sticks3/M5Unified` tag `0.2.21`：`src/M5Unified.cpp` 的 StickS3 配置和 `src/utility/Mic_Class.cpp` 的 I2S 配置

## Test Environment

Research firmware 使用单独的 PlatformIO environment：

```text
m5stack-sticks3-mic-research
JINGGUA_ENABLE_MIC_RESEARCH=1
```

普通的 `m5stack-sticks3` environment 不启用 microphone research。harness 不调用 Wi-Fi、云端 API、网络服务或上传路径。

启动时会短暂执行一次 `M5.Mic.begin()` 以验证配置，然后立即执行 `M5.Mic.end()`；启动阶段不采集样本。只有串口显式命令才开始一次固定时长采集。每次采集结束（成功或失败）都会调用 `M5.Mic.end()`，关闭 I2S、mic task 和 ES8311 analog path。

当前环境的 `platformio device list` 没有枚举可用 StickS3，因此下表中所有设备观测仍为 pending。

## Sample Rate

M5Unified 的 `mic_config_t.sample_rate` 是 `uint32_t`，没有一个公开的 StickS3 固定枚举列表；`Mic_Class` 会将 `sample_rate * over_sampling` 用于 I2S clock divider。当前 harness 将 `over_sampling` 固定为 1，所以 requested rate 不会被 software oversampling 改写。API/clock path 接受一个 rate 并不等于该 rate 已经在具体板卡上稳定通过，必须看设备日志和人工听感。

测试候选：

| Requested | 串口设置 | Actual / stable | 状态 |
| ---: | --- | --- | --- |
| 8,000 Hz | `r8000` | 由日志 `actual_rate_hz` 和 `rate_stable` 给出 | PENDING DEVICE VALIDATION |
| 16,000 Hz | `r16000` | 同上；首选语音候选 | PENDING DEVICE VALIDATION |
| 22,050 Hz | `r22050` | 同上；可选探测 | PENDING DEVICE VALIDATION |
| 44,100 Hz | `r44100` | 同上；可选探测 | PENDING DEVICE VALIDATION |
| 48,000 Hz | `r48000` | 同上；可选探测 | PENDING DEVICE VALIDATION |

`actual_rate_hz` 是 `sample_count / elapsed_ms` 的 wall-clock effective rate，不是外部时钟分析仪读数。`rate_stable=YES` 是 harness 的保守启发式：effective rate 与 requested rate 误差不超过 5%，且没有队列满观察、record failure 或 timeout。它不能替代真机示波器/逻辑分析仪验证。

## Buffer / RAM

默认配置为 `block_samples=256`、`dma_buffers=4`：

| 项目 | 数值/含义 |
| --- | --- |
| Application block | 256 samples × 2 bytes = 512 bytes |
| DMA descriptor 参数 | `dma_buf_len=256`、`dma_buf_count=4` |
| Logical mono DMA payload estimate | 4 × 256 × 2 = 2,048 bytes；不含 driver metadata/alignment |
| M5Unified I2S read chunk | `dma_buf_len × 2 × sizeof(int16_t)` = 1,024 bytes，源码用于 paired-slot read |
| Application recording slots | M5Unified 有 2 个异步 recording slots；本 harness 每次提交一块并等待完成后才复用 buffer |
| Temporary buffer | 显式使用 `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`，实际为 512 internal-RAM bytes |
| PSRAM temporary buffer | 0 bytes；本 harness 不把语音样本整段放入 PSRAM |

M5Unified 的 DMA allocation、FreeRTOS mic task stack 和 driver metadata 不是公共 API 可直接查询的完整 breakdown。harness 输出以下可复核指标，而不是伪造精确总量：

- `free_heap_before` / `free_heap_during_min` / `free_heap_after`
- `free_psram_before` / `free_psram_during_min` / `free_psram_after`
- `temp_buffer_bytes`、`internal_bytes`、`psram_bytes`
- `max_recording_slots`、`queue_full_observations`、`record_failures`、`record_timeouts`

`queue_full_observations` 是对 `M5.Mic.isRecording()==2` 的计数；M5Unified 对该返回值的定义是 recording queue 没有空位。它是调度压力代理指标，不是 I2S DMA underrun/overflow 硬件计数。M5Unified 公共 Mic API 没有暴露底层 DMA error counter，所以日志中不能把该代理为 0 解读成“硬件绝无丢样”。

## Recording Duration

harness 支持 `d<N>` 设置秒数，最大 60 秒；默认是 5 秒。最小验收矩阵必须覆盖：

| Duration | 命令 | 状态 |
| ---: | --- | --- |
| 5 s | `d5` | PENDING DEVICE VALIDATION |
| 15 s | `d15` | PENDING DEVICE VALIDATION |
| 30 s | `d30` | PENDING DEVICE VALIDATION |

每个 rate/duration 组合至少重复 5 次，记录完整串口日志。完整的自动统计并不代表音质合格；30 秒若 timeout、sample count 不足或 rate 不稳定，必须记录实际原因。

## Audio Quality

当前没有可用的 StickS3 音频样本，也没有实机，因此以下项目全部是 **PENDING DEVICE VALIDATION**：

- 安静房间：约 30 cm，发送 `a`，观察 ambient RMS/peak 和削波。
- 正常手持说话：设备离嘴 20–40 cm，发送 `v`，使用固定句子“今天适合做什么决定”。
- 瞬态：相同距离拍手或轻敲，发送 `t`，观察 peak 和 clipped count。
- 主观听感：清晰、基本可懂、噪声较大或不可接受，必须基于实际保存/回放的测试音频或等价的受控听音流程，不能只根据 RMS/peak 下结论。

本分支的 privacy-first harness 默认只在 RAM 中计算统计并丢弃 PCM，不生成音频文件。因此它可以验证输入路径、样本数量、level 和 clipping，但当前日志本身不能证明语音 intelligibility。任何后续主观听音实验都应使用明确 opt-in 的临时音频导出方案，并把样本留在仓库外。

## Storage / Encoding Estimate

对 `16 kHz / 16-bit / mono`：

```text
16,000 samples/s × 2 bytes/sample × 1 channel = 32,000 bytes/s
```

| Duration | Raw PCM bytes | Raw PCM KiB | WAV wrapper |
| ---: | ---: | ---: | ---: |
| 5 s | 160,000 | 156.25 KiB | 160,044 bytes（+44-byte header） |
| 15 s | 480,000 | 468.75 KiB | 480,044 bytes |
| 30 s | 960,000 | 937.50 KiB | 960,044 bytes |
| 60 s | 1,920,000 | 1,875.00 KiB | 1,920,044 bytes |

8 kHz 的上述 PCM 数值正好减半。编码取舍：

| Encoding | CPU/RAM | Storage | Complexity / decision |
| --- | --- | --- | --- |
| PCM S16LE | 最低；当前逐块处理只需 512-byte application block | 基准大小 | 当前 research 默认；最容易审计和交给后续处理 |
| WAV wrapper | 近似 PCM；只多 44-byte header，需在结束时回填长度 | PCM + 44 bytes | 简单、互操作性好；本分支不写文件 |
| IMA ADPCM | 需要 predictor/index 状态和 block framing；CPU/RAM 仍可控 | 约 1/4 PCM payload | 可作为未来本地临时存储实验，但尚未实现、尚未验证 STT 影响 |
| Opus/AAC | 压缩率更好，但 codec、调参、CPU、验证成本更高 | 更小 | 不在本 Issue 实现；不能以“支持 codec”代替真机输入验证 |

结论是：5–15 秒整段 PCM 在 PSRAM 容量上看似可行，30 秒约 0.96 MB 也小于标称 8 MB PSRAM；但可用 PSRAM、其他运行时占用、Flash/文件系统和功耗尚未实测，且整段缓存会把产品设计绑定到一个大的连续 buffer。更稳妥的设备端设计是小的 internal-RAM chunk buffer；若未来确需保存整段，再单独评估 PSRAM 或临时 Flash 文件。当前 harness 不实现保存或上传。

## Power

**NOT MEASURED / estimated only**：当前没有精密电流计，固件日志也不能推导 mA 数字。至少需要外部 USB-C 电流计分别记录：

1. 屏幕 active、无 microphone capture 的 idle 中位数；
2. `v` 采集期间的稳定 capture 中位数；
3. capture 增量和 5/15/30 秒期间是否有异常波动。

`M5PM1`/固件状态不能替代 USB 电流计；本分支不编造 idle 或 capture mA。

## Limitations

1. 当前没有连接 StickS3，8k/16k/其他 rate 的实际稳定性尚未验证。
2. 没有 5/15/30 秒真机日志，duration 是否完整、是否 timeout 尚未验证。
3. 没有测试音频，无法作安静房间噪声、正常手持语音可懂度或 clipping 的最终判断。
4. 没有电流计，Power 为 NOT MEASURED / estimated only。
5. effective rate 是 wall-clock proxy；`queue_full_observations` 不是底层 DMA overrun/underrun counter。
6. 当前 research capture 在主 loop 中同步逐块等待，采集期间会暂停主 loop 的 IMU poll、按钮 poll 和 UI render；这是 harness 限制，不是未来产品架构的结论。正式产品需另行设计 non-blocking capture 并验证 IMU/UI/battery 影响。
7. 没有实现 Flash temporary file、ADPCM、Speech-to-Text、Wi-Fi、API、云服务或上传。

## Privacy

- 默认不持续录音，不后台监听。
- 只有用户显式发送 `a`、`v` 或 `t` 才开始一次固定时长采集。
- 每次成功或失败都明确调用 `M5.Mic.end()`，并输出 `peripheral=stopped`。
- PCM 只在逐块 RAM buffer 中短暂存在，统计后丢弃；不写 Flash/SD，不上传。
- 测试音频、串口临时目录和研究产物不应提交 Git；`.gitignore` 覆盖 `*.wav`、`*.raw`、`*.pcm` 和 `.hw-validation-tmp/`。

## Manual Device Validation

在仓库根目录执行：

```powershell
python -m platformio run -e m5stack-sticks3-mic-research
python -m platformio run -e m5stack-sticks3-mic-research -t upload
python -m platformio device monitor -b 115200
```

串口 monitor 设置为 115200，并启用发送 newline。看到 `peripheral=stopped` 后：

1. 发送 `p`，确认默认 `requested_rate_hz=16000`、`duration_ms=5000`。
2. 发送 `r8000`，按 Enter；发送 `d5`，按 Enter；保持设备静置约 30 cm，发送 `a`。重复 5 次。
3. 发送 `r16000`，按 Enter；发送 `d5`，按 Enter；保持 20–40 cm，朗读“今天适合做什么决定”，发送 `v`。重复 5 次。
4. 在 `r16000` 下分别发送 `d15`、`d30` 后再发送 `v`；每个时长至少 5 次。若 30 秒失败，保存完整 failure 日志和原因。
5. 可选地对 `r22050`、`r44100`、`r48000` 重复 5 秒 `a`/`v`，只有实际日志稳定且听感可接受才标为 stable。
6. 另用 USB-C 电流计做 active-idle 与 capture 对照，填写 Power 表。
7. 若要做主观回放，必须另用仓外、明确授权的临时音频导出；不要把 `.wav`/`.raw`/`.pcm` 样本放入 Git。

建议把结果贴回 Issue/PR，至少包含：

```text
rate, duration_s, case, run, actual_rate_hz, rate_stable,
sample_count, rms_milli, peak, clipped, queue_full_observations,
record_failures, record_timeouts, free_heap_before,
free_heap_during_min, free_heap_after, free_psram_before,
free_psram_during_min, free_psram_after, subjective_quality,
power_idle_ma, power_capture_ma
```

## Recommendation

### Q1 — 是否足以支持用户主动按键后说一句 5–15 秒问题？

硬件路径和 PCM 资源预算显示“有条件可行”，但在没有 8/16 kHz 真机连续采集和 20–40 cm 主观听感之前，不能批准为 GO。需要先完成上述矩阵；若 5–15 秒都稳定且语音至少基本可懂，才可进入下一步设计。

### Q2 — 能否在设备本地把整段音频放 RAM？

5–15 秒的 16 kHz PCM 体积分别约 156 KiB / 469 KiB；8 MB PSRAM 从容量上看可能容纳。可是当前没有实测可用 PSRAM 和峰值占用，internal RAM 不应承担整段 buffer；30 秒虽然约 938 KiB，但不应仅凭标称容量承诺。结论：短时整段放 PSRAM **理论可行、设备验证 pending**；默认实现应使用 chunk buffer。

### Q3 — RAM buffer、Flash temporary file 还是 stream upload？

本 Issue 的最合理 research/prototype 方案是小的 internal-RAM chunk buffer，当前已经采用。Flash temporary file 需要文件系统、磨损和清理策略；stream upload 需要用户授权、网络和安全设计，属于 #14/后续 Issue，本分支不实现。未来若 #14 明确需要远端处理，优先在显式结束采集后采用受控 chunk streaming，而不是无限制整段 RAM；这只是设计建议，不是当前功能。

### Q4 — 是否会明显影响 IMU、UI、battery？

当前同步 harness 会明显暂停主 loop 的 IMU、按钮和 UI poll，这是代码结构造成的可见影响；不能据此断言正式产品一定冲突。I2S1 与 IMU 的 I2C 总线路径不同，但 DMA/CPU/heap 和 battery 影响仍需真机测量。功耗结论为 NOT MEASURED。

### Q5 — 是否适合未来 #14？

**条件适合，不能现在签 GO。** 如果 8/16 kHz、5–15 秒在五次重复中稳定，20–40 cm 语音基本可懂，资源余量和功耗可接受，则 StickS3 microphone path 可作为 #14 的输入前提；#14 仍需另行研究隐私授权、非阻塞采集、编码/传输和 STT，不在本 Issue 实现。

### Final Decision

**PENDING DEVICE VALIDATION**

通过条件：5–15 秒稳定、样本数量完整、无 timeout/明显队列压力、语音清晰或至少基本可懂、IMU/UI/battery 影响可接受。满足这些条件后，可将结论更新为 **GO WITH LIMITATIONS**（优先限制为不超过 15 秒、chunk buffer、显式触发）；若 5–15 秒满足并且资源/功耗余量充分，再单独评估是否 **GO**。若采集不稳定、噪声不可接受或存在资源冲突，应判定 **NO-GO**。
