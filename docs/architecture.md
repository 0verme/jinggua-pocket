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
- `Display` 封装 M5GFX 的清屏、文字和线段 primitive。

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
