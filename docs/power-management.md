# Power Management（Issue #5）

## 目标与范围

固件现在有独立的 `PowerManager`，负责可在主机上确定性测试的 inactivity
计时和电源状态；`StickS3PowerController` 负责把状态映射到 M5Unified 显示
和 ESP32-S3 light sleep。电源状态不修改 `AppState`，因此不会把低功耗策略
耦合到卦象领域逻辑。

当前状态机为：

```text
Active → Dim → Display Off → Light Sleep → Wake → Active
```

本 Issue 不伪造电流、续航或真机稳定性数据。

## 默认策略

| 状态 | 进入条件 | 硬件行为 | IMU 行为 |
| --- | --- | --- | --- |
| `Active` | 启动、用户活动或唤醒 | 正常亮度 `128` | 每个主循环轮询 |
| `Dim` | 无活动达到 `30 s` | 可读的低亮度 `32` | 每个主循环轮询 |
| `DisplayOff` | 无活动达到 `60 s` | `M5.Display.sleep()`，关闭背光/显示输出，MCU 仍运行 | 降为 `250 ms` 间隔轮询，可检测 Shake 唤醒 |
| `LightSleep` | 无活动达到 `300 s`（5 分钟） | 配置 BtnA/BtnB GPIO wake 后调用 `esp_light_sleep_start()` | 停止普通轮询 |

超时、亮度和 Display Off 后的 IMU 间隔全部集中在
`application::PowerPolicy`。`uint32_t` elapsed arithmetic 使用无符号减法，
可跨越 `millis()` wrap-around。

## 用户活动

以下行为调用 `PowerManager::recordActivity()`，将状态恢复为 `Active` 并
重新开始 inactivity epoch：

- 有效 BtnA / BtnB / LongPress 输入；
- 有效 Shake 触发；
- 开始一爻、reset、进入/离开设置；
- History 浏览；
- 用户主动 Wi-Fi 操作；
- Light sleep 唤醒。

普通 IMU sample 不算活动。

## Sleep inhibition

`StateMachine::sleepAllowed()` 是 application 层提供的清晰边界。以下关键
窗口返回 `false`，PowerManager 保持 `Active`，不会自动进入 Display Off 或
Light Sleep：

- 铜钱 animation 正在执行；
- `ResetConfirm`；
- `WifiConnecting`；
- `WifiConnected`（避免已连接射频在当前版本被低功耗路径打断）。

未来 OTA 或其他短生命周期任务应接入同一个 inhibition 边界，而不是直接
修改 PowerManager。

## Display 与 Light Sleep 的区别

- `Dim` 只改变背光亮度，显示仍可读；
- `DisplayOff` 调用 `M5.Display.sleep()`，MCU 仍在主循环中运行；
- `LightSleep` 先保持 Display Off，再由硬件 adapter 调用 ESP32 light-sleep
  API。它不是 `setBrightness(0)` 的别名。

唤醒后不会重新调用 `setup()`、不会重建 `DivinationSession` 或
`HistoryStore`；application RAM 中未完成卦象、已完成卦象和 cursor 都保留，
并强制重绘当前 `AppState`。

## Wake 策略

StickS3 的 M5Unified BtnA/BtnB 映射为 GPIO11/GPIO12，按键低电平有效。进入
light sleep 前，adapter 配置两个 GPIO 的 low-level wake；只配置 Button wake，
不实现 IMU motion interrupt wake。

唤醒后：

1. 读取并记录 ESP32 wake reason；
2. `PowerManager::wake()` 恢复 `Active` 并重置计时；
3. 恢复正常亮度并强制重绘；
4. `StickS3Buttons::ignoreUntilReleased()` 丢弃唤醒时的按住、click 或 hold，
   下一次新的按键才交给 App；
5. reset `ShakeDetector`，避免 wake 后重复生成一爻。

## IMU 功耗策略

BMI270 仍由 M5Unified 初始化。Active/Dim 保持当前摇动检测行为；Display Off
将普通 polling 降为 250 ms；Light Sleep 完全停止普通 polling。Display Off
仍可通过低频轮询检测 Shake，Light Sleep 的 IMU interrupt wake 留待后续单独
验证。

## Deep Sleep 决策

```text
Deep Sleep: DEFERRED
Deep Sleep != implemented unless verified.
```

Deep Sleep 会触发 reset，需要额外设计 RTC/NVS 状态恢复、当前未完成卦象的
原子保存、History 安全和唤醒 UX。本 Issue 只实现 light sleep，不把 Deep Sleep
误标为已实现。

## 真机验证矩阵

当前执行环境没有 StickS3 和电流计：

```text
PENDING DEVICE VALIDATION
```

| 项目 | 结果 |
| --- | --- |
| Active current | PENDING DEVICE VALIDATION |
| Dim current | PENDING DEVICE VALIDATION |
| Display Off current | PENDING DEVICE VALIDATION |
| Light Sleep current | PENDING DEVICE VALIDATION |
| wake latency | PENDING DEVICE VALIDATION |
| state restore（未完成/已完成卦象） | PENDING DEVICE VALIDATION |
| 10x sleep/wake cycles | PENDING DEVICE VALIDATION |
| Button wake false events | PENDING DEVICE VALIDATION |
| Display/背光恢复与可读性 | PENDING DEVICE VALIDATION |
| Display Off Shake 行为 | PENDING DEVICE VALIDATION |
| Light Sleep wake reason | PENDING DEVICE VALIDATION |

Native tests、firmware compile 和 adapter compile 不能替代以上真机验证。
