# State Machine

状态机位于 `application/state_machine.*`，只接收 `InputEvent`，不读取 GPIO，
不依赖显示 API。

## 状态

```text
BOOT
  │ begin()
  ▼
WELCOME ── PRIMARY_CLICK ──> PREPARE
  │
  │ SECONDARY_CLICK
  ▼
SETTINGS ── PRIMARY_CLICK ──> WIFI_CONNECTING ── update ──> WIFI_CONNECTED
  │                              │ update              │ update
  │ SECONDARY_CLICK              │ update              │
  ▼                              ▼                     ▼
WELCOME                    WIFI_FAILED        WIFI_TIMEOUT
                               │                    │
                               │ PRIMARY_CLICK      │ PRIMARY_CLICK
                               │ (retry)            │ (retry)
                               ▼                    ▼
                          WIFI_CONNECTING     WIFI_CONNECTING
```

SETTINGS: PrimaryClick → 开始连接（WifiController::enable()），SecondaryClick → Welcome，LongPress → 切换 runtime 声音开关。
WIFI_CONNECTING: SecondaryClick → 取消连接（WifiController::disable()）→ SETTINGS。
WIFI_CONNECTED: PrimaryClick → 关闭连接（WifiController::disable()）→ SETTINGS。
WIFI_FAILED: PrimaryClick → 重试 → WIFI_CONNECTING，SecondaryClick → SETTINGS。
WIFI_TIMEOUT: PrimaryClick → 重试 → WIFI_CONNECTING，SecondaryClick → SETTINGS。

## 输入事件扩展

| Event | 来源 | 新增用途 |
| --- | --- | --- |
| `SecondaryClick` | StickS3 `BtnB` | 从 Welcome 进入 Wi-Fi 设置；从 Wi-Fi 状态返回设置 |

Shake 在 Wi-Fi 页面不产生效果；Wi-Fi 连接状态不影响起卦流程。

## 电源状态（Issue #5）

电源状态与 `AppState` 正交，由 `PowerManager` 独立推进：

```text
Active ──30s idle──> Dim ──60s idle──> Display Off ──300s idle──> Light Sleep
   ▲                         ▲                                      │
   └──────────── Button / Shake / Wake activity ────────────────────┘
```

- `Active` 使用正常亮度，`Dim` 只降低亮度；
- `Display Off` 关闭显示输出但 MCU 仍运行，IMU 降频轮询；
- `Light Sleep` 停止普通 IMU polling，硬件 adapter 等待 BtnA/BtnB wake；
- `StateMachine::sleepAllowed()` 在 coin animation、`ResetConfirm`、
  `WifiConnecting` 和 `WifiConnected` 期间返回 false，PowerManager 保持
  `Active`；
- 唤醒只恢复显示和 inactivity timer，不改变 session、History 或当前
  `AppState`。

以下行为算作 PowerManager activity：有效按键、有效 Shake、开始一爻、reset、
进入/离开设置、History 浏览、主动 Wi-Fi 操作和 wake。普通 IMU sample 不算
activity。

## 输入事件

| Event | 来源 | v0.1 用途 | v0.2 新增用途 |
| --- | --- | --- | --- |
| `PrimaryClick` | StickS3 `BtnA` | 页面前进、Button 起一爻 | 连接/关闭 Wi-Fi |
| `SecondaryClick` | StickS3 `BtnB` | 取消重新起卦 | 从 Welcome 进入 Wi-Fi 设置，从 Wi-Fi 页面返回设置 |
| `LongPress` | M5Unified hold | 确认重新起卦 | — |
| `Shake` | `ShakeDetector` | Casting 状态起一爻；未完成时在 LineResult 直接进入下一爻 | — |
| `None` | 无输入 | 不改变状态 | — |

电源逻辑不把 `setBrightness(0)` 视为 Light Sleep：Display Off 使用显示适配器
的 sleep/wakeup，Light Sleep 另外调用 ESP32-S3 light-sleep API。Button wake
后的按住/点击事件会被丢弃，避免唤醒动作重复推进 App。

ShakeDetector 默认启用，使用加速度/角速度门限、释放门限、500 ms 检测窗口、
80 ms 最小峰间隔和 1200 ms cooldown。只在 Casting 或未完成的 LineResult
采样；六爻完成后会重置检测器，因此后续摇动不会继续生成爻。Button 仍可在
所有原有页面作为 fallback 使用。

声音 cue 由 `AudioController` 端口发出：开始一轮播放 `Start`，前五爻播放
`Cast`，第六爻用一次 `Complete` 替代 `Cast`；Wi-Fi 失败/超时进入错误态时
播放一次 `Error`。动画期间输入被忽略，因此不会产生音频请求洪泛。

## 渲染约束

状态机只维护 `dirty` 标记。输入导致状态或会话改变后，主循环渲染一次并
acknowledge；渲染不会改变领域数组。UI 显示六爻时从 `index 5` 到 `index 0`
遍历，领域顺序保持 `index 0 = 初爻`。

Renderer 的每个 `AppState` 都有明确 screen path。`Casting` 显示当前爻序和
三枚铜钱提示；`LineResult` 在 `CoinAnimation` active 时只等待，finished 后
显示铜钱、阴/阳、静爻/动爻和下一步。无动爻时停留在本卦结果，不进入空的之卦
screen。History 只呈现记录顺序、本卦、动爻和存在时的之卦；Wi-Fi 页面把
Off/Connecting/Connected/Failed/Timeout 映射为 `未连接`、`正在连接…`、`已连接`
和 `连接失败` 等产品文案，不泄露内部错误码。
