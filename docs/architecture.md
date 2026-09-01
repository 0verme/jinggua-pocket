# Architecture

## 目标

Phase 0 的目标不是堆功能，而是让六爻核心能在没有 StickS3 的情况下被验证，
并让真实硬件只承担适配职责。

## 分层

```text
┌──────────────────────────────────────┐
│ UI: Renderer / Screens                │  只绘制，不计算卦
├──────────────────────────────────────┤
│ Application: StateMachine / Session  │  编排事件与一次起卦会话
├──────────────────────────────────────┤
│ Domain: Coin / Yao / Trigram / ...   │  纯 C++，可独立测试
├──────────────────────────────────────┤
│ Data: trigram table / hexagram table │  静态基础映射
└──────────────────────────────────────┘
             ▲                 ▲
             │ ports           │ adapters
   RandomProvider       StickS3 Display/Buttons/IMU
  WifiController                Esp32WifiManager
```

### Domain

`domain/` 定义 `CoinSide`、`CoinResult`、`Yao`、`Trigram`、`Hexagram` 和
`DivinationResult`。它不包含 `M5Unified.h`、Arduino API、GPIO、屏幕坐标或
业务状态，也不直接生成随机数。

### Application

`DivinationSession` 注入 `RandomProvider`，一次调用 `castLine()` 固定消耗
3 枚铜钱，并把结果放入 `index 0..5 = 初爻..上爻`。`StateMachine` 只处理
`InputEvent`，因此按钮、IMU、未来的 BLE/Serial 都能被替换而不改变状态逻辑。

### Hardware

`hardware/` 是 StickS3 的边界：

- `Esp32RandomProvider` 使用 ESP32 entropy API；业务层看不到 `esp_random()`。
- `StickS3Buttons` 将 M5Unified `BtnA/BtnB` 映射到 `InputEvent`。
- `StickS3Imu` 将 M5Unified `IMU_Class` 数据转换为平台无关的 `ImuSample`。
- `ShakeDetector` 只处理样本、窗口和 cooldown，不知道应用状态。
- `Esp32WifiManager` 实现 `WifiController` 接口：默认 Off（离线优先），
  只有用户显式触发才 `WiFi.begin()`；连接由主循环 `update(nowMs)` 非阻塞
  推进，15 秒超时，失败/超时后不自动重连。凭据只通过构建期环境变量
  注入，不提交到仓库。

### UI

`Renderer` 根据 `AppState` 选择 screen；六爻绘制时故意按 `5 -> 0` 显示，
所以屏幕上上爻在最上方、初爻在最下方，而不修改领域数组。

## 依赖规则

1. Domain 不依赖 Hardware/UI。
2. Application 不读取 GPIO，不调用 Arduino `random()`，不绘制屏幕。
3. Hardware 只把外部输入和能力转换成接口/值。
4. UI 不反向修改 `Yao` 或卦象。
5. 静态卦表通过 `data/` 提供，未来完整卦辞可独立进入资源/API。

## v0.1 数据流

```text
M5.BtnA / M5.BtnB
        │
        ▼
InputEvent::PrimaryClick
        │
        ▼
StateMachine ── cast ──> DivinationSession
                              │
                              ▼
                         RandomProvider
                              │ 3 tosses
                              ▼
                 CoinResult -> Yao -> 6 lines
                              │
                              ▼
             original + moving lines + transformed
                              │
                              ▼
                         Renderer / Display
```

初始化时没有 Wi-Fi 初始化、网络请求、遥测或用户内容存储路径。

## Wi-Fi（Issue #7 新增）

`application/wifi_controller.h` 定义 `WifiState`（Off/Connecting/Connected/
Failed/Timeout）与 `WifiController` 接口（ports）；`hardware/wifi_manager.cpp`
的 `Esp32WifiManager` 是对 ESP32 Arduino `WiFi` 的适配器（adapter）。

离线优先是本模块的硬性边界：

- 开机、进入首页、开始起卦、Shake、Button 起卦、查看本卦/之卦、设备
  空闲都不会调用 `WiFi.begin()`；启动路径没有自动连接。
- 连接只能从 `Settings` 页面由用户按 A 显式触发；连接中按 B 取消、
  已连接按 A 主动关闭，都不存在后台自动重连。
- 连接是异步的：`enable()` 只发起 `WiFi.begin()`，主循环通过
  `StateMachine::update(nowMs)` 轮询推进，不阻塞 loop，没有
  `while (WiFi.status() != WL_CONNECTED) { delay(...) }`。

状态归属：`AppState` 扩展了 `Settings / WifiConnecting / WifiConnected /
WifiFailed / WifiTimeout`，由 `StateMachine` 处理；`Renderer` 按状态渲染
最小 UI，见 `docs/state-machine.md`。
