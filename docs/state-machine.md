# State Machine

状态机位于 `application/state_machine.*`，只接收 `InputEvent`，不读取 GPIO，
不依赖显示 API。

## 状态

```text
BOOT
  │ begin()
  ▼
WELCOME ── PRIMARY_CLICK ──> PREPARE
                               │ PRIMARY_CLICK
                               ▼
                            CASTING
                               │ PRIMARY_CLICK / SHAKE
                               │ cast one line
                               ▼
                         LINE_RESULT
                          │ PRIMARY_CLICK → CASTING (not complete)
                          │ SHAKE → cast next line (not complete)
              ┌───────────┴───────────┐
              │ not complete          │ complete
              ▼                       ▼
           CASTING              HEXAGRAM_RESULT
                                      │ PRIMARY_CLICK
                         ┌────────────┴────────────┐
                         │ has moving lines        │ no moving lines
                         ▼                         ▼
                  TRANSFORMED_RESULT          RESET_CONFIRM
                         │ PRIMARY_CLICK          │ PRIMARY_CLICK / LONG_PRESS
                         ▼                         ▼
                    RESET_CONFIRM              PREPARE
```

`RESET_CONFIRM` 收到 `SECONDARY_CLICK` 会返回进入确认前的结果页，避免误触
丢掉当前结果。确认重置才调用 `DivinationSession::reset()`。

## 输入事件

| Event | 来源 | v0.1 用途 |
| --- | --- | --- |
| `PrimaryClick` | StickS3 `BtnA` | 页面前进、Button 起一爻 |
| `SecondaryClick` | StickS3 `BtnB` | 取消重新起卦 |
| `LongPress` | M5Unified hold | 确认重新起卦 |
| `Shake` | `ShakeDetector` | Casting 状态起一爻；未完成时在 LineResult 直接进入下一爻 |
| `None` | 无输入 | 不改变状态 |

ShakeDetector 默认启用，使用加速度/角速度门限、释放门限、500 ms 检测窗口、
80 ms 最小峰间隔和 1200 ms cooldown。只在 Casting 或未完成的 LineResult
采样；六爻完成后会重置检测器，因此后续摇动不会继续生成爻。Button 仍可在
所有原有页面作为 fallback 使用。

## 渲染约束

状态机只维护 `dirty` 标记。输入导致状态或会话改变后，主循环渲染一次并
acknowledge；渲染不会改变领域数组。UI 显示六爻时从 `index 5` 到 `index 0`
遍历，领域顺序保持 `index 0 = 初爻`。
