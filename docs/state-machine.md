# State Machine

状态机位于 `application/state_machine.*`，只接收 `InputEvent`，不读取 GPIO，
不直接构造 HTTP request。Wi-Fi 生命周期由 `WifiController` 管理，JingGua
上传由 `ApiClient` 端口管理。

## 状态与主路径

```text
BOOT
  │ begin()
  ▼
WELCOME ── PRIMARY_CLICK ──> PREPARE
  │                              │ PRIMARY_CLICK
  │ SECONDARY_CLICK              ▼
  ▼                          CASTING ── castLine() ──> LINE_RESULT
SETTINGS ── PRIMARY_CLICK ──> WIFI_CONNECTING             │
  │                         │ update                     │ PRIMARY_CLICK
  │ SECONDARY_CLICK         ▼                            ▼
  ▼                    WIFI_CONNECTED               HEXAGRAM_RESULT
WELCOME                    │ PRIMARY_CLICK              │ SECONDARY_CLICK
                           ▼                            ▼
                       SETTINGS                    UPLOADING
                                                        │ update
                                      ┌─────────────────┴─────────────────┐
                                      ▼                                   ▼
                                UPLOAD_SUCCESS                      UPLOAD_FAILED
                                      │ A/B                               │ A retry
                                      ▼                                   └─> UPLOADING
                                  结果页                               B → 原结果页
```

Wi-Fi 失败路径为 `WIFI_FAILED` / `WIFI_TIMEOUT`，只由用户再次按 A 重试；不自动
reconnect。Wi-Fi 连接成功后按 B 进入设置，再按 B 返回首页，连接状态保持，供
结果页的显式上传使用。Settings 的 LongPress 切换 runtime 声音开关；
WIFI_CONNECTING 的 SecondaryClick 取消连接，WIFI_CONNECTED 的 PrimaryClick
关闭 Wi-Fi，失败/超时状态只允许用户按 A 重试或按 B 返回设置。

## 状态说明

| State | 含义 | 关键输入 |
| --- | --- | --- |
| `Boot` | 初始状态 | `begin()` → `Welcome` |
| `Welcome` | 首页 | A → `Prepare`；B → `Settings` |
| `Prepare` | 默念问题但不输入/记录问题 | A → `Casting` |
| `Casting` | 等待生成下一爻 | A 或 Shake → `castLine()` |
| `LineResult` | 展示刚生成的一爻 | 动画结束后 A → 下一爻或 `HexagramResult` |
| `HexagramResult` | 展示本卦和动爻 | A → 之卦/重卦；B → `Uploading` |
| `TransformedResult` | 展示之卦 | A → 重卦；B → `Uploading` |
| `Uploading` | 已由用户请求，等待单次有界 API call | 输入忽略；`update()` 完成一次请求 |
| `UploadSuccess` | 服务器接受结果 | A/B → 原结果页 |
| `UploadFailed` | 上传失败，保留本地结果 | A → 单次手动 retry；B → 原结果页 |
| `ResetConfirm` | 确认重新起卦 | A/长按 → `Prepare`；B → 原结果 |
| `History` | 离线历史浏览 | A/B 移动；长按 → `Welcome` |
| `Settings` | Wi-Fi 设置 | A → 连接/查看连接；B → `Welcome` |
| `WifiConnecting` | 异步连接中 | B → 关闭 Wi-Fi 并返回设置 |
| `WifiConnected` | Wi-Fi 已连接 | A → 关闭；B → 设置 |
| `WifiFailed` | 连接失败 | A → 手动重试；B → 设置 |
| `WifiTimeout` | 连接超时 | A → 手动重试；B → 设置 |

## Offline First 不变量

```text
boot
  → Esp32WifiManager::begin() 将 radio 置为 WIFI_OFF
cast
  → DivinationSession::castLine() 只调用 RandomProvider
complete divination
  → result 和 history 先在本地存在
用户明确连接 Wi-Fi
  → WifiController::enable()
用户明确在结果页按 B 上传
  → 只有 WiFiState::Connected 才进入 Uploading/API capability
```

状态机不会在 `boot`、`castLine()`、单爻完成、渲染或后台 `update()` 中自动开启
Wi-Fi。Wi-Fi 未连接时结果页的上传操作直接得到 `ApiError::Offline`，不调用
`ApiClient`，也不尝试偷偷连接。

## API 调用边界

`StateMachine` 只持有 `ApiClient*`，不依赖 `HTTPClient`、`WiFiClientSecure`
或 JSON 细节。上传流程如下：

1. 结果页按 B，确认会话已经有完整 `DivinationResult` 且 Wi-Fi 为 Connected。
2. 切换到 `Uploading`；本次用户操作只排队一次调用。
3. 下一次 `update()` 调用 `ApiClient::beginUpload(const DivinationSession&, ...)`，
   后续 update 只轮询 `ApiClient::poll()`。
4. 根据完成的 `ApiResult` 切换到 Success 或 Failed；不自动重试。
5. Failed 只有用户再次按 A 才会创建下一次调用。

客户端收到的是 `const DivinationSession&`，并只读取六爻。API 失败、超时、HTTP
500 或解析失败均不会 reset、删除或改写本地会话。上传完成后本地
`HistoryRecord::syncStatus` 仍为 `Pending`；完整历史同步属于 #12。

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

| Event | 来源 | 用途 |
| --- | --- | --- |
| `PrimaryClick` | StickS3 `BtnA` | 页面前进、起一爻、连接、关闭、手动上传 retry |
| `SecondaryClick` | StickS3 `BtnB` | Wi-Fi 设置、结果页显式上传、返回 |
| `LongPress` | M5Unified hold | 重新起卦确认、历史退出、切换 runtime 声音开关 |
| `Shake` | `ShakeDetector` | Casting 起一爻；未完成时在 LineResult 进入下一爻 |
| `None` | 无输入 | 不改变状态 |

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
遍历，领域顺序保持 `index 0 = 初爻`，而 API payload 也保持
`index 0 = 初爻` 到 `index 5 = 上爻`。

Renderer 的每个 `AppState` 都有明确 screen path。`Casting` 显示当前爻序和
三枚铜钱提示；`LineResult` 在 `CoinAnimation` active 时只等待，finished 后
显示铜钱、阴/阳、静爻/动爻和下一步。无动爻时停留在本卦结果，不进入空的之卦
screen。History 只呈现记录顺序、本卦、动爻和存在时的之卦；Wi-Fi 页面把
Off/Connecting/Connected/Failed/Timeout 映射为 `未连接`、`正在连接…`、`已连接`
和 `连接失败` 等产品文案，不泄露内部错误码；上传页面只显示简短结果状态。
