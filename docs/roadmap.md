# Roadmap

路线按硬件成熟度推进，不提前实现复杂云端能力。每个仓库的变更保持独立，
Pocket 与 JingGua Web 不混在同一个 Git 工作树或 commit 中。

## v0.1 — Hardware v0.1 离线骨架

- M5Stack StickS3 / ESP32-S3
- Button 起卦
- 三枚铜钱法
- 初爻到上爻的六爻模型
- 本卦、上卦、下卦、卦序、卦名
- 动爻和之卦
- 离线运行
- Host native tests 与 PlatformIO firmware build

## v0.2 — 真机体验

- IMU Shake Mode 真机校准
- 轻量铜钱翻转动画
- 音效（在明确 scope 后）
- **135×240 Pocket UI polish（Issue #6）**：统一布局/字体层级、起卦流程、
  本卦/动爻/之卦、History 与 Wi-Fi 页面；真机视觉验收仍待完成。
- **用户触发的离线优先 Wi-Fi 流程（Issue #7）**：设置页显式连接、
  非阻塞异步、15 秒超时、失败/超时不自动重连、用户主动关闭。
- **自动息屏与低功耗管理（Issue #5）**：`Active → Dim → Display Off →
  Light Sleep → Wake` 的逻辑与 Button wake request path 已实现；真实设备
  电流和 wake stability 验证仍为 `PENDING DEVICE VALIDATION`。

## v0.3 — Connected JingGua

- **Issue #8：Pocket → JingGua API 最小 Contract 已实现**：
  - 六爻先在 ESP32 本地完整生成；API 只接收已完成结果；
  - 复用 Web `POST /api/divinations` 的 `line_values`、本卦编号、动爻和之卦编号；
  - `JingGuaApiClient` / `HttpTransport` / `ApiResult` ports 与 fake native tests；
  - HTTPS、CA 校验、bounded payload/response、timeout、错误分类和手动 retry；
  - 不发送问题、MAC、芯片唯一 ID 或长期 credential。
- **Issue #9：Device Binding**（未在本 Issue 实现）
- **Issue #10：QR Code**（未在本 Issue 实现）
- 手机查看完整解卦（待 API/产品契约另行确定）

## v0.4 — 同步

- **Issue #12：历史记录自动补同步**（未在 #8 实现）
- Web / Device 同步

## v0.5 — 语音入口

- **Issue #14：Voice**（未在本 Issue 实现）
- 麦克风
- 语音问事

## v1.0 — 稳定产品

- 定制外壳
- 稳定固件
- OTA（未在本 Issue 实现）
- 正式 Release

## 当前明确不规划

本路线不承诺 PCB v2、自定义电路、其他 M5Stack 兼容板、AI 解卦或完整经典
文本的固件内置方案；这些要在未来需求明确后另行评估。

## Issue #8 边界

本期数据流严格为：

```text
ESP32 RNG
  ↓
DivinationSession
  ↓
complete local hexagram
  ↓
optional user-triggered Wi-Fi
  ↓
JingGua API
  ↓
server record
```

服务器不能参与随机过程。不开 Wi-Fi 仍可启动、起卦、查看结果和保存本地历史；
API 失败不会阻止本地起卦。自动历史补同步、绑定、二维码、语音、OTA 和自动 Wi-Fi
均延期到各自 Issue。

## 后续 Bring-up

> **StickS3 UI visual validation**

Issue #6 的代码与 native layout coverage 已完成；在设备上验证字体、动画和
各页面边界后，再关闭 **PENDING DEVICE VISUAL VALIDATION**。随后按以下顺序
继续硬件 Bring-up：

1. 第一次刷机并确认串口日志。
2. 验证屏幕方向、清屏、文字与六爻线段。
3. 验证 `BtnA` / `BtnB` 的点击与长按事件。
4. 读取 BMI270 加速度/角速度样本，记录静止与摇动数据。
5. 校准 threshold/window/cooldown。
6. 将完整六爻 Button 流程和最小 Shake 流程跑上真机。
7. 按 [`docs/power-management.md`](power-management.md) 完成 Active、Dim、
   Display Off、Light Sleep 电流及 10 次 sleep/wake 矩阵。
8. 确认 Button wake 无伪 click、状态恢复和 IMU idle polling 行为。
9. 使用 `JINGGUA_API_URL` 与公开 `JINGGUA_API_ROOT_CA` 构建配置验证 HTTPS，
   再在 local fake server / mocked transport 验证 Contract，不宣称生产 E2E。
10. 真机验证“开机 radio off → 用户连接 → 结果页 B 上传 → 失败手动 retry”；
    #9 credential 与 #12 sync 另行设计，不把其状态提前塞进 #8。
