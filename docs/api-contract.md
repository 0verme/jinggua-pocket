# JingGua API Contract（Pocket v0.3 / Issue #8）

状态：`v1` 最小上传契约，先实现 Pocket 侧；REAL END-TO-END: PENDING。

## 1. 现有 Web 契约研究结论

本次基于 `0verme/jinggua` 的 `origin/main`（研究快照：`260ee9f`）检查了
Pages Functions、校验器、六爻算法和 D1 写入路径。

Web 当前已经提供可复用的：

```text
POST /api/divinations
Content-Type: application/json
```

它接收 `line_values`，使用同一套 6/7/8/9 六爻模型重新推导本卦、动爻和变卦，
并校验客户端可选发送的卦序字段。记录使用 `divination_id` 作为幂等键，数据库
通过 `INSERT OR IGNORE` 避免同一 ID 重复写入。成功响应当前为
`{"ok":true,"id":"..."}`。

当前 Web 没有 Pocket 专用的 API version 路径、设备绑定、匿名分享 token 或
`device` 持久化字段；也没有必要为了 Issue #8 复制一套六爻模型。因此 Pocket
使用这个现有 endpoint 的扁平字段，不修改 `0verme/jinggua`。

## 2. Randomness Boundary

六爻只能在 ESP32 本地完成：

```text
ESP32 hardware entropy
        ↓
DivinationSession::castLine()
        ↓
6 个完整的 domain::Yao
        ↓
DivinationResult（本卦 / 动爻 / 之卦）
        ↓
用户明确触发上传
        ↓
POST /api/divinations
```

API 只接收已经完成的 `line_values`，绝不调用服务器起卦随机数，也不能修改
`DivinationSession`。`castLine()` 不调用 API；生成单爻时不上传。

## 3. 六爻表示与顺序

`line_values` 是**恰好 6 个整数**，顺序固定为从下到上：

| index | position | 含义 |
| ---: | ---: | --- |
| 0 | 1 | 初爻 |
| 1 | 2 | 二爻 |
| 2 | 3 | 三爻 |
| 3 | 4 | 四爻 |
| 4 | 5 | 五爻 |
| 5 | 6 | 上爻 |

数值编码不可用布尔数组替代：

| value | 类型 | 阴阳 | 是否动爻 | 变后 |
| ---: | --- | --- | --- | --- |
| 6 | 老阴 | 阴 | 是 | 阳 |
| 7 | 少阳 | 阳 | 否 | 阳 |
| 8 | 少阴 | 阴 | 否 | 阴 |
| 9 | 老阳 | 阳 | 是 | 阴 |

Pocket 的 `Yao::coins.total` 直接映射为 `line_values[index]`。服务器使用同样的
规则校验输入；本卦和之卦由六爻推导，不由网络结果决定。

## 4. v1 Request

Pocket 发送扁平的、与现有 Web endpoint 兼容的 JSON：

```json
{
  "api_version": "v1",
  "divination_id": "pocket-00000001",
  "local_record_id": 1,
  "line_values": [6, 7, 8, 9, 8, 6],
  "hexagram_number": 40,
  "moving_lines": [1, 4, 6],
  "changed_hexagram_number": 41,
  "method": "coin"
}
```

字段规则：

- `api_version`：Pocket 的契约标记，当前 Web endpoint 尚未强制读取；如果未来
  响应明确返回其他版本，Pocket 拒绝该响应。
- `divination_id`：稳定的本地上传/幂等标识。优先由已持久化的
  `HistoryRecord::localRecordId` 生成 `pocket-<id>`；没有历史存储时使用本次
  完整六爻的稳定 fingerprint。它不是 credential，也不是分享 token。
- `local_record_id`：可选的数字本地记录 ID，给未来 #12 历史同步保留；#8 不实现
  sync engine。HTTP 同时预留 `Idempotency-Key`，值与 `divination_id` 相同。
- `line_values`：必填，来自已完成的 `DivinationSession`，严格为 6/7/8/9。
- `hexagram_number`：Pocket 本地 `DivinationResult::original.number`（1..64）。
  Web 会从 `line_values` 重算并校验一致性。
- `moving_lines`：`DivinationResult::movingPositions` 中的 1-based position，按
  从初爻到上爻升序排列。没有动爻时为 `[]`。
- `changed_hexagram_number`：Pocket 本地 `transformed.number`；没有动爻时为
  `null`。Web 会重算并校验一致性。
- `method`：固定为现有 Web 接受的 `coin`。

为了复用当前 Web 模型，`original_hexagram` / `changed_hexagram` 不再另造嵌套
对象；分别使用现有的 `hexagram_number` / `changed_hexagram_number`。卦名、卦辞、
完整爻对象也不重复上传。

### 有意不发送的字段

- `question` / `question_text`：Pocket #8 没有问题输入与同意流程，不发送、不保存。
- `visitor_id` / `session_id`：Pocket 不采集 Web 匿名访客标识。
- `device_id`、MAC、芯片唯一 ID、binding credential：#9 的正式 Binding 范围，
  #8 不发送。
- `created_at`：当前 StickS3 没有有效 RTC；Web 使用服务器接收时间。不要伪造
  时间戳。
- 长期 token、密码、私钥：#8 不实现 credential。

Pocket request body 上限为 512 bytes；超过上限在发送前安全失败。

## 5. Response

Pocket 接受现有 Web 的兼容响应：

```json
{"ok":true,"id":"pocket-00000001"}
```

并预留未来更明确的最小响应：

```json
{
  "api_version": "v1",
  "record_id": "...",
  "status": "accepted"
}
```

成功只表示服务器接受并记录了已完成的本地结果。Pocket 不要求返回解释文本，
不实现 QR，也不把 `record_id` 当作长期 credential。响应 body 上限为 512 bytes；
空 body、非法 JSON、超大 body、缺少必要成功字段或版本不匹配都视为可恢复失败。

## 6. Error Model

Application 层至少区分以下错误；UI 统一显示“上传失败 / 稍后重试”，详细枚举只写
serial debug log，且不输出 URL 中的 credential 或响应 token：

```text
Offline
WiFiUnavailable
Timeout
TransportError
Http4xx
Http5xx
InvalidResponse
ApiVersionMismatch
```

映射规则：

- Wi-Fi 未处于 `Connected`：`Offline`（没有 API call）。
- Wi-Fi controller 不可用：`WiFiUnavailable`。
- transport 超时：`Timeout`。
- DNS/TLS/连接/其他传输失败：`TransportError`。
- HTTP `400..499`：`Http4xx`。
- HTTP `500..599`：`Http5xx`。
- body 不符合上述响应：`InvalidResponse`。
- 响应带有非 `v1` 的 `api_version`：`ApiVersionMismatch`。

## 7. Offline First 与触发方式

硬性流程：

```text
boot
  → Wi-Fi OFF
cast
  → local only
complete divination
  → local result exists（并先写入本地 history）
user explicitly requests Wi-Fi
  → Wi-Fi CONNECTED
user explicitly requests upload
  → API capability available
```

约束：

- 开机不自动打开 Wi-Fi。
- `castLine()` 不触发 API。
- 不逐爻上传。
- API 失败、超时或解析失败都不能删除/改变本地 `DivinationSession` 或阻止
  本地卦象展示。
- 不后台无限 retry，不自动 reconnect。
- 上传只由结果页的用户操作触发；失败后只有用户再次按 Retry 才会发起一次新请求。
- #12 的历史自动补同步不在本契约实现范围。

结果页的 `B` 用于上传；Wi-Fi 需要用户先从设置页显式连接。Wi-Fi 未连接时不会
偷偷连接，上传直接得到 `Offline`。失败页的 `A` 是一次手动 retry，`B` 返回结果。

## 8. HTTP / TLS 安全边界

- 只允许 HTTPS endpoint；Pocket 不使用 `setInsecure()`，不关闭证书校验。
- ESP32 transport 设置有限的 connect/read timeout，并限制 request/response body；
  每次用户上传只运行一个 one-shot request task，主循环通过 poll 取得结果；不使用
  无限 retry 或后台 reconnect。
- CA certificate 通过构建环境的 `JINGGUA_API_ROOT_CA` 注入；未配置 CA 时 API
  transport 安全地不可用，而不是降级为不校验证书。正式 CA 内容不是 secret，仍不
  应在日志中输出。
- endpoint URL 通过非敏感的 `JINGGUA_API_URL` 构建配置注入；仓库不写死生产
  credential。#9 再定义正式 binding credential。
- 请求不包含 MAC、芯片唯一标识、问题原文或长期 credential。
- 不打印请求 body、Idempotency-Key 或响应 body；仅记录有限的错误类别。

## 9. Web 仓库策略

本次研究确认 `0verme/jinggua` 已有足够的 `POST /api/divinations` endpoint，
所以没有修改 Web 仓库，也没有创建 Web 分支/commit/PR。Pocket 只适配其现有
`line_values` 模型；若未来需要显式版本、设备字段或分享 token，应另开 Web Issue
和独立 PR，不能与本 Issue 混提交。

## 10. 测试 Contract

Native tests 使用 fake `WifiController`、fake `ApiClient` 和 fake `HttpTransport`，
覆盖：Wi-Fi Off 不调用 API、显式联网后成功上传、timeout、HTTP 500、非法 JSON、
超大响应、手动 retry，以及 API begin/poll 前六爻已经完整生成。测试还验证 API
client 只读取 `const DivinationSession`，不能改变本地会话；上传启动与轮询都不阻塞
状态机输入路径。

REAL END-TO-END: PENDING（本 Issue 使用 fake transport/local contract，不宣称生产
服务端联调通过）。
