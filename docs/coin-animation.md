# 三枚铜钱动画

## 数据流

`DivinationSession::castLine()` 先通过 `RandomProvider` 生成三枚铜钱，并将
`CoinResult` 保存到最新的 `Yao`。起卦成功后，`Renderer` 把这份已经固定的
结果传给 `ui::CoinAnimation`；动画模块不依赖、也不调用任何随机 API。

动画使用 `Idle`、`Spinning`、`Settling`、`Finished` 四个状态。总时长约
720 ms，由 `update(now_ms)` 非阻塞推进；三枚铜钱共享一轮动画并使用很小的
phase offset。`widthPercent` 仅控制横向半径：动画过程中可压缩到 2 px 模拟侧面，
静止目标半径为 13×13 px。动画结束后 UI 直接绘制最终 `CoinResult`，并复用同一
铜钱绘制组件，保证静态结果仍是完整正圆而不是旧椭圆或中间帧。

## 铜钱视觉

- 显示使用现有 M5GFX primitive，不加载 bitmap、不分配动态内存。
- 135×240 屏幕上每枚钱币直径 26 px，中心约为 x=31、67、103 px，中心间距
  36 px、相邻边缘约留 10 px；提示、动画和结果使用同一中心与尺寸。
- 主体为暖铜橙色，深铜棕外缘、低强度金色内缘/短高光；中央使用 5×5 px 黑色方孔。
- `Front` 保留业务编码并显示四个小金色点饰；`Back` 保留业务编码并显示两道深铜竖向刻纹。两面不交换映射，动画停止帧仍逐枚等于 `CoinResult`。

## 输入与性能

动画期间 `StateMachine` 保持最新一爻不变并拒绝新的输入，避免一轮动画生成
第二爻。主循环暂停 `ShakeDetector` 的采样但不调用 `reset()`，所以已有的
shake cooldown 不会被动画清除；动画结束后恢复采样。动画只使用 3 个小型
frame 值和显示 primitive，不分配 bitmap 或堆内存。

当前实现每个动画帧重绘一次结果页，帧率受主循环现有的 20 ms 调度间隔限制。
动画在 135×240 portrait 上使用统一的三枚铜钱中心和纵向安全区，标题保持在
动画上方，不被铜钱遮挡。这是 2D 的宽度缩放效果，不是真正的 3D 翻转；真机
上仍需进行视觉节奏和面板可读性的确认。
