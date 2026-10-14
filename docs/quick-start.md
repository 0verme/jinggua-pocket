# v0.3.0 累积预览版快速开始与限制

> v0.3.0 是首次累积**预览版**，不是正式 `v1.0`，软件测试也不等于 StickS3 真机验收。Issue #18 保持 OPEN；显示、按钮、IMU、声音、低功耗、外壳和 OTA 实机验证仍须单独完成。

## 目标设备与下载

目标设备为 M5Stack StickS3 / ESP32-S3。GitHub Pre-release 提供：

- `sticks3-usb-flash-v0.3.0.zip`：含 bootloader、partition table、`boot_app0`、`firmware.bin` 和 Windows 烧录脚本；普通 USB 烧录建议使用此包。
- `firmware.bin`：应用分区镜像，也用于 OTA manifest。
- `firmware-manifest.txt`：版本、大小、SHA-256 和固件下载 URL。
- `SHA256SUMS.txt`：发布资产校验和。

## Windows + StickS3 USB 烧录

1. 从 [v0.3.0 Release](https://github.com/0verme/jinggua-pocket/releases/tag/v0.3.0) 下载 USB 烧录 ZIP 与 `SHA256SUMS.txt`，解压 ZIP。
2. 在 PowerShell 中将 ZIP 哈希与 `SHA256SUMS.txt` 对照：

   ```powershell
   $line = Get-Content .\SHA256SUMS.txt | Where-Object { $_ -match '\s+sticks3-usb-flash-v0\.3\.0\.zip$' }
   if (-not $line) { throw 'ZIP checksum entry missing' }
   $expected = $line.Substring(0, 64)
   $actual = (Get-FileHash .\sticks3-usb-flash-v0.3.0.zip -Algorithm SHA256).Hash.ToLowerInvariant()
   if ($actual -ne $expected) { throw 'ZIP SHA-256 mismatch; do not flash' }
   Write-Host 'SHA-256 verified'
   ```

3. 安装 Python 3 / esptool（已有 `py -3 -m esptool` 时可跳过）：

   ```powershell
   py -3 -m pip install "esptool>=4.8,<5"
   ```

4. 使用 USB-C 数据线连接 StickS3，在设备管理器确认实际 COM 号；不要假定固定端口。进入解压目录后执行：

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\flash_windows.ps1 -Port COM5
   ```

   将 `COM5` 替换为本机端口。脚本按 ESP32-S3 `default_8MB.csv` 的偏移写入 bootloader、partition table、`boot_app0` 和 app 镜像；**不运行 `erase_flash` 整片擦除**，但 esptool 会按需重写目标镜像范围内的 flash sectors。NVS 位于独立分区，不在这些写入范围内；`boot_app0` 会初始化 OTA 选择数据。只对 M5Stack StickS3 使用该包。若端口无法自动进入下载模式，按 M5Stack 官方 StickS3 烧录说明进入下载模式，不要通过整片擦除来“修复”。

5. 烧录完成后复位设备；串口监视器使用 115200 baud。首次启动检查欢迎页、A/B 输入、屏幕、声音和离线起卦；任何失败均记录复现步骤与串口日志。

`firmware.bin` 是 app-slot 镜像，不是从 flash offset `0x0` 开始的整片镜像；不要把它单独写到 `0x0`。如果直接使用 esptool 写 app-slot，必须先确认目标设备的 bootloader/partition layout 与本项目兼容。发布流程不会代替用户烧录，也不会执行真实设备擦写。

## 离线起卦

1. 欢迎页按 A 进入准备页，再按 A 开始。
2. 在起卦中短按 A 仍为逐爻起卦；按 A 约 1 秒可启动一次自动补全，从当前 0～5 爻补到六爻。松开后流程会自行继续，动画逐爻串行，显示进度并在完成后进入本卦结果页。
3. 起卦期间 Shake 仍可逐爻；自动补全期间重复按键和 Shake 会被忽略。最终结果显示本卦、动爻、离线 `CODE NNNN`；有动爻时按 A 查看之卦。
4. 首页的 B 入口打开 Wi-Fi 设置。版本页显示 `0.3.0`（编译版本最长支持 24 个 ASCII 字符）；设置页可长按切换声音。

自动流程期间睡眠被抑制。DisplayOff 唤醒所用的 A 长按会被消费；松开后需重新长按才开始自动补全。实际阈值手感、唤醒事件和屏幕进度均待真机验收。

## 联网与 OTA

Wi-Fi 默认关闭，只有从设置页主动连接；结果页 B 才会发起已完成卦象 API 上传。凭据通过构建配置注入，不要提交真实 SSID、密码、API URL/Token 或私有 CA。设备绑定、二维码分享、历史云同步和产品语音流程尚未交付；麦克风 PR 是硬件/录音研究，不是语音识别功能。

OTA 需先连接 Wi-Fi，再由用户显式进入检查/安装。设备固定查询 `releases/latest`；GitHub 通常不会把 Pre-release 作为 `latest`，所以 v0.3.0 的 OTA manifest/固件资产**不能被该 OTA 地址自动发现**。本预览可通过 USB 包烧录。真实下载、取消、断电恢复、rollback 与版本切换仍为 `PENDING_HARDWARE_VALIDATION`。SHA-256 是镜像完整性校验，不是独立代码签名。更多细节见 [`ota.md`](ota.md)。

## 已知限制与验收边界

- 0～5 爻自动补全、长按手感/误触、动画严格串行/观感、135×240 中文布局、声音和最终跳转尚无真机验收。
- IMU Shake、BtnB、息屏/Light Sleep 唤醒、电流/续航和 NVS 物理故障恢复须在真实 StickS3 上验收。
- OTA 真实下载/取消/掉电/rollback 未在设备验证；v0.3.0 prerelease 不被固定 OTA `releases/latest` 发现。
- #15 只有 CAD 草案和验证计划；未声称外壳 render/实体通过。正式配网、设备绑定、二维码、历史同步、语音和 v1.0 均未完成。
- **当前主分支许可证**：本项目原创内容已采用 [Apache License 2.0](../LICENSE)；第三方资料遵循各自原有许可证。`v0.3.0` 发布时的历史记录未被改写，以当前仓库根目录 `LICENSE` 为最新声明。

所有未实测硬件项统一保持 `PENDING_HARDWARE_VALIDATION`。本项目也不是医疗、法律、财务或其他专业建议的替代品。
