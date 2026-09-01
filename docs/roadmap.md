# Roadmap

路线按硬件成熟度推进，不提前实现复杂云端能力。

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

## v0.3 — 用户触发的 Web 联动

- Wi-Fi credential provisioning（BLE / 二维码 / Web 配网）
- `jinggua` API
- 设备绑定
- 二维码
- 手机查看完整解卦

## v0.4 — 同步

- 历史记录
- Web / Device 同步

## v0.5 — 语音入口

- 麦克风
- 语音问事

## v1.0 — 稳定产品

- 定制外壳
- 稳定固件
- OTA
- 正式 Release

## 当前明确不规划

本路线不承诺 PCB v2、自定义电路、其他 M5Stack 兼容板、AI 解卦或完整经典
文本的固件内置方案；这些要在未来需求明确后另行评估。

## 下一步

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
