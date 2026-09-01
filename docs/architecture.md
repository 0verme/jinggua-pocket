# Architecture

## 目标

JingGua Pocket 保持 Offline First：设备本地完成随机起卦、本卦、动爻和之卦；
网络只负责在用户明确操作后接收已经完成的结果。真实硬件只承担适配职责，
application/domain 可以在没有 StickS3 的情况下测试。

## 分层

```text
┌─────────────────────────────────────────────────────┐
│ UI: Renderer / Screens                              │  只绘制，不计算卦
├─────────────────────────────────────────────────────┤
│ Application: StateMachine / Session / ApiClient     │  编排本地流程与可选上传
├─────────────────────────────────────────────────────┤
│ Domain: Coin / Yao / Trigram / Hexagram / Result    │  纯 C++，可独立测试
├─────────────────────────────────────────────────────┤
│ Data: trigram table / hexagram table                │  静态基础映射
└─────────────────────────────────────────────────────┘
             ▲                    ▲
             │ ports              │ adapters
  RandomProvider / WifiController / ApiClient / HttpTransport
             ▲                    ▲
   ESP32 entropy / Wi-Fi       Esp32HttpsTransport
```

### Domain

`domain/` 定义 `CoinSide`、`CoinResult`、`Yao`、`Trigram`、`Hexagram` 和
`DivinationResult`。它不包含 `M5Unified.h`、Arduino API、GPIO、屏幕坐标或
HTTP；不直接生成随机数。六爻数组固定为 `index 0 = 初爻`、`index 5 = 上爻`，
每条爻值固定为 6/7/8/9。

### Application

- `DivinationSession` 注入 `RandomProvider`。每次 `castLine()` 固定消耗 3 枚
  本地铜钱，六次完成后才生成 `DivinationResult`。
- `StateMachine` 只处理 `InputEvent`，管理 Wi-Fi 状态和用户触发的上传状态；
  它不写 HTTP request，也不依赖 Arduino 网络类。
- `JingGuaApiClient` 将完整的、只读的 `DivinationSession` 映射为现有 Web
  `/api/divinations` 的扁平 JSON；`ApiResult` 将 transport、HTTP 和解析错误
  归一化为 UI 不必暴露的错误类别。
- `HttpTransport` 是可替换的单次 POST port；native tests 使用 fake transport
  或 fake `ApiClient`，不访问生产服务。

### Hardware

`hardware/` 是 StickS3 边界：

- `Esp32RandomProvider` 使用 ESP32 entropy API；业务层看不到 `esp_random()`。
- `StickS3Buttons` 将 M5Unified `BtnA/BtnB` 映射到 `InputEvent`。
- `StickS3Imu` 和 `ShakeDetector` 只处理平台输入样本。
- `Esp32WifiManager` 实现 `WifiController`：启动时明确设置 `WIFI_OFF`，只有
  用户显式触发才 `WiFi.begin()`；连接由主循环 `update(nowMs)` 推进，15 秒超时，
  失败/超时后不自动重连。
- `Esp32HttpsTransport` 只接受 HTTPS，使用构建注入的 CA certificate 验证服务端，
  设置有界 connect/read timeout、request/response 上限和 `Idempotency-Key`；
  没有 CA 时 fail closed，绝不调用 `setInsecure()`。每次用户上传只创建一个
  one-shot FreeRTOS request task，完成后由主循环 poll；没有后台 retry/reconnect。

### UI

`Renderer` 根据 `AppState` 选择 screen；六爻绘制时故意按 `5 -> 0` 显示，
所以屏幕上上爻在最上方、初爻在最下方，而不修改领域数组。上传页面只显示
“上传中 / 上传成功 / 上传失败，稍后重试”，不显示 credential、token 或服务端
原始响应。

## 依赖规则

1. Domain 不依赖 Hardware、UI、HTTP 或 Wi-Fi。
2. Application 不读取 GPIO，不调用 Arduino `random()`，不绘制屏幕。
3. `StateMachine` 只依赖 `WifiController` / `ApiClient` ports，不直接写网络。
4. Hardware 将外部输入、Wi-Fi、TLS 和 HTTP 转换为 application interfaces。
5. UI 不反向修改 `Yao`、`DivinationSession` 或卦象。
6. 静态卦表通过 `data/` 提供；服务器结果不能替换本地随机结果。

## 数据流与 Randomness Boundary

```text
ESP32 hardware entropy
        ↓
RandomProvider → DivinationSession::castLine()
        ↓  六次、每次三枚铜钱
完整 local DivinationResult
  (lines / original / moving / transformed)
        ├──────────────→ 本地 Renderer
        └──────────────→ 本地 HistoryRecord（先保存）
                              ↓
               用户明确打开 Wi-Fi 并在结果页按 B
                              ↓
        JingGuaApiClient → HttpTransport → HTTPS JingGua API
                              ↓
                    server divination record
```

服务器只能验证并记录已经完成的 `line_values`。它永远不参与上半段随机过程，
也不能改变 `DivinationSession`。问题原文、访客 ID、MAC、芯片唯一 ID 和长期
credential 不在 Issue #8 的请求中。

## Wi-Fi 与 API（Issue #8）

离线优先是不变量：

- boot 时 `Esp32WifiManager::begin()` 将 radio 置为 `WIFI_OFF`；没有自动连接。
- 开始起卦、每次 `castLine()`、动画、查看本卦/之卦和设备空闲都不调用 API。
- 只有 Settings 页用户按 A 才开始连接；连接中按 B 取消，失败/超时只能按 A
  手动重试。
- 结果页按 B 才请求上传；Wi-Fi 不是 `Connected` 时不会调用 `ApiClient`。
- 一次请求有界失败后进入 `UploadFailed`；没有后台无限 retry。只有用户按 A
  才再次请求。历史自动补同步属于 #12。

`docs/api-contract.md` 记录了与 `0verme/jinggua` 现有
`POST /api/divinations` 的字段映射、6/7/8/9 编码、兼容 response、TLS、上限、
错误模型和测试边界。本 Issue 不修改 Web 仓库；如未来需要 device binding、QR
或正式 API version endpoint，必须另开 Web PR。
