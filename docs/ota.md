# 安全固件更新（Issue #16）

## 软件状态与硬件边界

Pocket OTA 是用户显式触发的单次 HTTPS 更新流程。检查更新、下载、校验和重启均有独立 UI 状态；开机、Wi-Fi 连接、起卦和待机不会自动检查或下载。没有 CA 配置时 OTA fail closed，离线起卦、NVS 历史和普通固件构建不受影响。

PlatformIO `m5stack-sticks3` 当前使用 ESP32-S3 `default_8MB.csv`：`ota_0` 与 `ota_1` 各 0x330000 bytes，`otadata` 独立，NVS 独立。固定 PlatformIO ESP32-S3 `qio_opi` SDK config 中 `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1`。CI 的 `tools/check_ota_support.py` 检查这组分区/bootloader 前提；构建后也检查固件不超过单 OTA slot。

安装时 `Update` 只写入 inactive OTA app slot；整个镜像通过 HTTPS 获取、长度限制和 SHA-256 检查、ESP image 校验后才激活 boot partition。取消请求与激活用同一 critical section 仲裁：若 B 取消先到，写入任务中止并调用 `Update.abort()`，当前 boot slot 不变；若完整性校验后进入 `Finalizing` 先到，界面切为不可取消的「正在更新/请勿断电」，随后完成 `Update.end(false)`，避免 UI 仍显示可取消时偷偷切换 boot slot。激活失败会回到错误页，不会卡在重启页。写入前的网络失败不开始写 flash。OTA 不写 NVS / 历史分区。更新后新镜像启动时先完成基础显示与应用状态机初始化，再调用 `esp_ota_mark_app_valid_cancel_rollback()`；基础启动失败则请求 `esp_ota_mark_app_invalid_rollback_and_reboot()`，交由已启用的 ESP-IDF bootloader 回退。

SHA-256 用于镜像完整性验证，不是离线签名。HTTPS CA 校验与 GitHub Release 仓库/写权限是当前发布信任边界；尚未配置独立离线签名密钥或 Secure Boot。不能把 SHA-256 文案误称为独立发布者签名。

## 用户流程

1. 用户先从欢迎页进入 Wi-Fi 设置并主动连接网络。
2. 已连接页长按任一按键打开「固件更新」确认页。
3. 按 A 才查询版本；发现新版本后必须再次按 A 才开始下载/安装。
4. 下载过程中按 B 请求安全取消；界面显示进度。SHA 与 ESP image 校验通过后原子关闭取消入口，进入不可取消的 finalization 屏幕，激活结果成功后才显示重启提示。
5. 错误显示有限类别；只有用户再次按 A 才会重试。不会自动连接 Wi-Fi、重试或更新。

OTA 查询需要 Wi-Fi 当前处于 Connected。缺少配置会显示“更新服务未配置”，不会跳过证书验证。Wi-Fi Settings 页显示当前编译版本（`JINGGUA_FIRMWARE_VERSION`，开发默认 `0.2.0`）；OTA 检查页显示 manifest 中的最新/候选版本，两者含义不同。设置页的软件显示路径有 host/build coverage，但没有 StickS3 实屏/发布版本验收，仍为 `PENDING_HARDWARE_VALIDATION`。更新/取消/finalization 期间 `StateMachine::sleepAllowed()` 返回 false，减少屏幕休眠中断操作。

## Manifest / GitHub Release 契约

设备只读取固定地址：

```text
https://github.com/0verme/jinggua-pocket/releases/latest/download/firmware-manifest.txt
```

Manifest 是不超过 512 bytes 的 ASCII `key=value` 文件，必须完整包含且只包含以下字段：

```text
format=1
version=1.0.0
size=1234567
sha256=<64 lowercase hex characters>
url=https://github.com/0verme/jinggua-pocket/releases/download/v1.0.0/firmware.bin
```

版本必须为无前导零的数字 `major.minor.patch`，且严格大于当前编译版本；镜像大小必须为 1..0x330000 bytes；URL 必须严格匹配同版本的本仓库 GitHub Release `firmware.bin` 路径。未知字段、重复字段、非 HTTPS/外站 URL、超限内容和不匹配的 hash 都拒绝。GitHub Release 下载 redirect 最多跟随三次，并限制到本仓库 manifest 或
`release-assets.githubusercontent.com` / `objects.githubusercontent.com`；TLS 每跳仍由配置 CA 校验。

Release build 在 PlatformIO 环境注入非敏感版本与公开 CA：

```sh
JINGGUA_FIRMWARE_VERSION=1.0.0 \
JINGGUA_OTA_ROOT_CA="-----BEGIN CERTIFICATE-----..." \
python -m platformio run -e m5stack-sticks3

python tools/create_ota_manifest.py \
  --firmware .pio/build/m5stack-sticks3/firmware.bin \
  --version 1.0.0 \
  --output firmware-manifest.txt
```

创建 GitHub Release 时应将 `firmware.bin` 和该 manifest 作为同一 tag 的 release assets。`tools/create_ota_manifest.py` 只生成 manifest，不创建/发布 Release，也不触碰生产环境。本次基线检查时仓库还没有任何 GitHub Release；因此 OTA 更新包会在 #18 发布准备生成之前不可用，这是发布前置，不影响当前本地固件的离线起卦。

## 可复验软件测试

Native 测试覆盖 manifest 大小/字段/重复项/版本/hash/URL/slot 边界、语义版本比较、仅在用户按键后检查/安装、断网不调用服务、失败后的显式 retry、进度、取消以及 OTA 不改变起卦会话。`check_ota_support.py` 检查目标 SDK 的 rollback 和双分区，而非只检查应用宏。

本地完整验证：

```sh
cmake -S test/native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release --parallel
ctest --test-dir build/native --build-config Release --output-on-failure
pio run -e m5stack-sticks3
python tools/check_ota_support.py
python tools/check_font_asset.py
python tools/check_format.py
git diff --check
```

## `PENDING_HARDWARE_VALIDATION`

NAS 没有 StickS3，以下项目未作 PASS 声明，集中在用户最终烧录验收：

- 真实 Release 更新可下载、版本切换和启动后版本号；
- 下载中断/取消后旧固件继续启动，历史记录完整；
- 在擦写 inactive slot 各阶段断电/重启，旧 slot 可启动；
- 损坏 hash / 无效镜像被拒绝，新固件基础启动失败时 bootloader rollback；
- 成功更新后 bootloader 确认新镜像并结束 rollback window；
- 长时间 Wi-Fi/电源状态、用户反馈与屏幕进度显示。

固件构建通过、Native fake、CI 或分区表检查均不能代替以上真机证据。
