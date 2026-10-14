# JingGua Pocket 更新日志

本日志记录代码/固件交付；真机验收状态单独标记，不把软件测试当作硬件 PASS。

## v0.3.0 — 首次累积预览版

本版本没有上一条成功 Release/Tag 作为基线，汇总截至审计基线的全部 main 历史和 21 个已合并 PR，而非只包含 Issue #42。完整 PR 标题、合并时间、head branch、merge commit、分类与收录判断见 [`v0.3.0 发布审计`](docs/releases/v0.3.0-audit.md)；中文 Release Notes 见 [`v0.3.0 发布说明`](docs/releases/v0.3.0.md)。

### 新增功能

- StickS3 / ESP32-S3 离线六爻起卦闭环、IMU Shake、六十四卦映射与本卦/动爻/之卦结果。
- A 短按逐爻；A 长按约 1000 ms 自动补齐剩余爻，保留既有爻，逐轮串行播放动画并自动进入本卦结果页。
- 离线历史持久化、离线卦象码、用户主动触发的 Wi-Fi/API、显式检查/安装的安全 OTA 软件路径。

### 交互、视觉与稳定性

- 三枚铜钱非阻塞动画、视觉/旋转/结束姿态、135×240 小屏与 Noto Sans SC 字体、克制的起卦声音反馈。
- 自动息屏与低功耗路径、历史 NVS 恢复与启动 reset reason 记录、OTA 取消/激活竞态修复、按键与状态机测试覆盖。
- 版本显示、开发预览/烧录文档、Python 3 开发命令、OTA Manifest/分区检查与完整 Native/PlatformIO 自动化测试。

### 能力边界

- Wi-Fi 默认关闭；上传需用户显式触发。没有设备绑定、二维码同步、历史云同步或产品语音流程；麦克风 PR 是研究，不代表 STT/语音问事已交付。
- v0.3.0 是 prerelease。固定 OTA 地址 `releases/latest` 通常不选择 prerelease，所以本版通过 USB 资产提供烧录；不声称设备可通过该固定地址在线发现本版。
- 本次未进行 StickS3 真机烧录/验收。长按手感、动画/显示、IMU、声音、功耗、NVS 物理行为和 OTA 真实更新均为 `PENDING_HARDWARE_VALIDATION`。

## Unreleased

暂无未归档的产品变更。
