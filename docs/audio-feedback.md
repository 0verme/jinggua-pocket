# 声音反馈

## 目标与边界

JingGua Pocket 的声音只用于轻量确认：轻、短、安静、有确认感，不模拟
游戏音效。实现使用 M5Unified `Speaker_Class::tone()` 的合成正弦波，不加入
WAV、旋律、语音、STT 或麦克风录音资源。

应用层只发出 `SoundCue`，不依赖 `M5.Speaker`。`StateMachine` 通过
`application::AudioController` 端口调用 `StickS3AudioController` 硬件适配器。

## Sound Cue

| Cue | 触发 | 实现 | 说明 |
| --- | --- | --- | --- |
| `Start` | `Prepare` 按 A 开始一轮 | 260 Hz / 24 ms | 极短低音 |
| `Cast` | 非最后一爻成功生成 | 440 Hz / 28 ms | 每爻只确认一次，不按三枚铜钱分别播放 |
| `Complete` | 第六爻成功生成 | 520 Hz / 22 ms + 720 Hz / 36 ms | 非常短的上扬双音 |
| `Error` | 进入 `WifiFailed` / `WifiTimeout` | 180 Hz / 32 ms | 低、短，不使用 alarm；普通无效按键保持静音 |

第六爻使用 `Complete` 替代 `Cast`，因此同一个手势不会同时重复播放两个
确认音。默认 M5Unified master volume 为 `48/255`，实际听感和音量仍需真机
确认。

## 非阻塞与队列策略

M5Unified 的 `tone()` 把请求交给后台 `spk_task`；应用层不调用
`delay(500)`，也不等待声音结束。适配器固定使用一个 virtual channel，并在
`isPlaying(channel) == 2`（当前音频和待播槽都占用）时丢弃新请求；普通确认
使用 `stop_current_sound=true`，让最新交互优先。这样快速输入不会无限累积
音频请求，也不会阻塞 CoinAnimation、Shake、Button、Wi-Fi 或状态机。

`Complete` 的第二个音只在第一个音成功进入队列后以非抢占方式追加；若队列
已满则退化为单音确认，不制造等待。

## Mute

`StateMachine::setSoundEnabled(bool)` 和 `toggleSoundEnabled()` 提供 runtime
全局开关。带 Wi-Fi 的 Settings 页面保留原有 A/B 操作，LongPress 切换声音，
屏幕显示「声音：开/关」。关闭时硬件适配器停止当前 channel 并将 master volume
设为 0。

本 PR **不持久化**声音设置：当前为 runtime-only，重启后恢复开启。这样避免
扩大现有 Preferences/NVS 历史存储范围；后续若需要记忆用户选择，可单独增加
一个设置 key。

## StickS3 硬件审计

项目锁定 M5Unified `0.2.21`。实际编译依赖中的 `M5Unified.cpp` 对
`board_M5StickS3` 配置如下：

| 路径 | MCLK | BCLK | WS/LRCK | Data | I2S |
| --- | ---: | ---: | ---: | ---: | --- |
| ES8311 speaker TX | GPIO18 | GPIO17 | GPIO15 | DOUT GPIO14 | `I2S_NUM_0` |
| ES8311 microphone RX | GPIO18 | GPIO17 | GPIO15 | DIN GPIO16 | `I2S_NUM_1` |

Speaker 配置为 22,050 Hz、stereo、非 buzzer、非 DAC；M5Unified 的 StickS3
speaker callback 通过内部 I2C/PM1 打开音频电源并写入 ES8311 DAC 配置。音频
路径不是裸 GPIO PWM，也不由本仓库猜测引脚。参考
<https://docs.m5stack.com/en/arduino/m5sticks3/speaker> 与锁定依赖的
`M5Unified.cpp` / `Speaker_Class.hpp`。

`Display::begin()` 负责唯一一次 `M5.begin(config)`：

- 普通 `m5stack-sticks3`：`internal_mic=false`、`internal_spk=true`；声音
  控制器只读取已配置的 `M5.Speaker`，首次 cue 时由 M5Unified lazy begin
  speaker task/I2S TX。
- `m5stack-sticks3-mic-research`：`internal_mic=true`、`internal_spk=false`；
  研究 harness 继续拥有 ES8311 ADC/I2S RX，产品声音路径明确关闭。

当前实现不在同一运行模式中同时使用 Speaker 和 Mic。虽然两者使用不同的
I2S port，但共享 ES8311、MCLK/BCLK/WS 与 codec I2C 状态；M5Unified 的两个
callback 也会分别改写 codec 的 DAC/ADC 时钟和电源寄存器。因此麦克风研究
环境不会被意外启动 Speaker，普通固件也不会意外启动 Mic；同时录放音属于
明确的 documented limitation，而不是本 PR 的承诺。

## 验证记录

Native FakeAudioController 覆盖：

- Start exactly once；
- 非末爻 Cast exactly once；
- 六爻完成时 Complete exactly once，且不重复 Cast；
- mute 时 0 次 hardware playback；
- 无效按键不产生错误音，Wi-Fi failure 只确认一次；
- 动画期间 100 次快速输入仍只接受一爻，不产生 audio queue flood。

真机矩阵：

| 项目 | 状态 |
| --- | --- |
| Start / Cast / Complete / Error cue | PENDING DEVICE AUDIO VALIDATION |
| mute | PENDING DEVICE AUDIO VALIDATION |
| 连续 10 次 cast | PENDING DEVICE AUDIO VALIDATION |
| Shake + animation + audio 并发 | PENDING DEVICE AUDIO VALIDATION |
| speaker volume / harshness / clipping | PENDING DEVICE AUDIO VALIDATION |

当前执行环境没有连接 StickS3，因此不能把编译通过写成音量、削波或听感通过。

## Flash / RAM

使用同一台 Windows 主机、PlatformIO `espressif32@6.12.0`、Release 构建，
对比 `origin/main` 在改动前的结果：

| Environment | 改动前 Flash / RAM | 改动后 Flash / RAM | 增量 |
| --- | ---: | ---: | ---: |
| `m5stack-sticks3` | 1,004,205 B / 47,348 B | 1,033,389 B / 47,388 B | +29,184 B / +40 B |
| `m5stack-sticks3-mic-research` | 1,028,293 B / 47,420 B | 1,042,345 B / 47,444 B | +14,052 B / +24 B |

Flash 增量包含新增音频适配器、日志、Settings 文案以及为新增文案重新生成的
VLW 字形子集（287 glyphs / 40,885 B）；没有新增 WAV。PlatformIO 的静态
RAM 数字不包含运行时 Speaker task、I2S DMA 和 codec 的完整动态 breakdown，
因此仍需真机资源与听感验证。
