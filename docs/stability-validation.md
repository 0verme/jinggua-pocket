# Firmware Stability Validation

Issue #17 的软件加固与物理设备验收分开记录。Native/PlatformIO 通过不代表真机 soak、功耗或传感器行为通过。

## 已实现的软件防护

- 启动时记录 ESP-IDF reset reason 和数值码；分类包括 panic、interrupt/task/other watchdog、brownout、software reset、power-on、deep-sleep 和 unknown。应用不重配置、关闭或自行喂养 framework watchdog，避免掩盖卡死。
- 历史存储在任何 NVS 读写前显式打开 `jinggua` Preferences namespace。初始化失败时 `HistoryStore::begin()` 返回失败，history 操作 fail closed，不阻断本地起卦。
- record/index CRC 与损坏恢复由 Native tests 覆盖；启动校验 active records 顺序及 active range 外的孤儿记录，以恢复“记录写入成功、index 提交失败”的情况。恢复写入、擦除或 index 持久化失败时不宣称 store 可用，之后一次启动可重试恢复。
- Wi-Fi/API 保持有限超时、错误分类、手动单次重试；网络失败不丢弃本地起卦结果。
- ShakeDetector、PowerManager、按钮唤醒抑制、音频与显示状态恢复的应用逻辑有 Native coverage；它们不是物理传感器/屏幕测量。

## 必须真机完成（当前均未执行）

| 验证 | 状态 | 备注 |
| --- | --- | --- |
| 长时间 soak 与 heap trend | `PENDING_HARDWARE_VALIDATION` | 需要实际 StickS3、串口记录和明确时长；编译不能代替 |
| Watchdog/panic reset 与重新启动 | `PENDING_HARDWARE_VALIDATION` | 先在隔离设备上触发/观察；本任务不注入故意崩溃代码 |
| Battery、电流与 sleep/wake 循环 | `PENDING_HARDWARE_VALIDATION` | 含 battery behavior、唤醒稳定性及显示恢复 |
| NVS 断电/故障注入 | `PENDING_HARDWARE_VALIDATION` | Native fake 可验证返回路径，不模拟 ESP32 Flash/NVS 物理特性 |
| IMU false trigger、button debounce | `PENDING_HARDWARE_VALIDATION` | Native 算法测试不能替代设备校准与机械输入 |
| Audio/display 故障恢复 | `PENDING_HARDWARE_VALIDATION` | 需实际外设故障场景与可视/可听验收 |

没有 StickS3 的运行环境中，不把上述项目标记为通过，也不以它们阻塞可自动化的软件交付。
