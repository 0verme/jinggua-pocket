# 三枚铜钱动画

## 数据流

`DivinationSession::castLine()` 先通过 `RandomProvider` 生成三枚铜钱，并将
`CoinResult` 保存到最新的 `Yao`。起卦成功后，`Renderer` 把这份已经固定的
结果传给 `ui::CoinAnimation`；动画模块不依赖、也不调用任何随机 API。

动画使用 `Idle`、`Spinning`、`Settling`、`Finished` 四个状态。总时长约
720 ms，由 `update(now_ms)` 非阻塞推进；三枚铜钱共享一轮动画并使用很小的
phase offset。`widthPercent` 控制椭圆的横向半径，铜钱在收窄到侧面后再展开
为目标正面或反面。

## 输入与性能

动画期间 `StateMachine` 保持最新一爻不变并拒绝新的输入，避免一轮动画生成
第二爻。主循环暂停 `ShakeDetector` 的采样但不调用 `reset()`，所以已有的
shake cooldown 不会被动画清除；动画结束后恢复采样。动画只使用 3 个小型
frame 值和显示 primitive，不分配 bitmap 或堆内存。

当前实现每个动画帧重绘一次结果页，帧率受主循环现有的 20 ms 调度间隔限制。
动画在 135×240 portrait 上使用统一的三枚铜钱中心和纵向安全区，标题保持在
动画上方，不被铜钱遮挡。这是 2D 的宽度缩放效果，不是真正的 3D 翻转；真机
上仍需进行视觉节奏和面板可读性的确认。
