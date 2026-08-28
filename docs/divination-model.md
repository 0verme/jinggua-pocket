# Divination Model

## 三枚铜钱

每枚铜钱是 `CoinSide`：

- `Front = 3`
- `Back = 2`

`CoinResult::fromCoins()` 保存三枚铜钱原始顺序和总数；四种总数映射为：

| Total | YaoType | 阴阳 | 动爻 | 变后 |
| ---: | --- | --- | --- | --- |
| 6 | 老阴 | 阴 | 是 | 阳 |
| 7 | 少阳 | 阳 | 否 | 阳 |
| 8 | 少阴 | 阴 | 否 | 阴 |
| 9 | 老阳 | 阳 | 是 | 阴 |

## 六爻顺序

`DivinationSession` 每次使用 `lineCount + 1` 作为 position：

```text
index 0 / position 1 = 初爻
index 1 / position 2 = 二爻
index 2 / position 3 = 三爻
index 3 / position 4 = 四爻
index 4 / position 5 = 五爻
index 5 / position 6 = 上爻
```

这个内部顺序永远不因 UI 改变。屏幕绘制才反向遍历，使上爻在上、初爻在下。

## 八卦

每个 `Trigram` 保存三条从下往上的线，`Yang = 1`、`Yin = 0`，并提供
`TrigramId`、中文名和 binary value：

| 卦 | Id | 从下往上 | binary value |
| --- | --- | --- | ---: |
| 乾 | Qian | 阳阳阳 | 7 |
| 兑 | Dui | 阳阳阴 | 3 |
| 离 | Li | 阳阴阳 | 5 |
| 震 | Zhen | 阳阴阴 | 1 |
| 巽 | Xun | 阴阳阳 | 6 |
| 坎 | Kan | 阴阳阴 | 2 |
| 艮 | Gen | 阴阴阳 | 4 |
| 坤 | Kun | 阴阴阴 | 0 |

`createHexagram()` 从 `lines[0..2]` 得到下卦，从 `lines[3..5]` 得到上卦。

## 六十四卦

`data/hexagrams.cpp` 是基础映射表，保存 King Wen 卦序、卦名、上卦和下卦。
不包含卦辞、爻辞、象传、彖传或 AI 内容。`DivinationResult` 保存：

- `original`
- `movingPositions`
- `movingCount`
- 有动爻时才有 `transformed`

变卦使用每个 `Yao::transformedYinYang`，静爻保持，动爻翻转；它复用同一
组六爻的位置，不创建反向数组。

## 随机性

`RandomProvider` 是 application port。生产实现使用 ESP32 hardware entropy；
Host tests 使用固定 sequence fake。这样可以测试：

```text
固定 coin sequence -> 固定 CoinResult -> 固定 Yao -> 固定卦象
```

领域核心不直接调用 Arduino `random()`。
