# 虚拟运动控制与设备动画系统

由于当前开发环境没有真实运动控制卡，项目使用软件方式完整模拟工业设备的运动、运输、动画过程。

## 虚拟运动控制器 (VirtualMotionController)

8 轴平台模拟，支持梯形加减速、软限位、急停/报警/复位，由 60fps tick 驱动：

- **Conveyor** — 传送带位置（mm），板子跟随此轴移动
- **Stopper** — 挡板状态（0=下降, 1=上升）
- **CameraX / CameraY** — 相机头 XY 位置
- **LaserX / LaserY** — 激光头 XY 位置
- **Z** — Z 轴（预留）
- **R** — 旋转轴（预留）

每个轴独立建模 (`AxisModel`)：当前位置、目标位置、速度 (mm/s)、加速度 (mm/s²)、软限位、运动状态 (Idle/Moving/Homing/Done/Alarm/EmergencyStopped)。

`tick(deltaSec)` 按梯形加减速曲线更新各轴位置，到达目标后自动切换为 Done 并触发 `AxisDoneCallback`。

## 虚拟运输控制器 (VirtualTransportController)

闭环运输模拟，状态机驱动：

- `loadBoard()` → 传送带以 200mm/s 运送板子
- 板子到达挡板位置 (450mm) → 自动切换 `BoardReady`，升起挡板
- `unloadBoard()` → 板子继续前进至出口 → 回到 `Idle`
- `tick(deltaSec)` 每帧更新板位置，通过 `IMotionController` 同步到动画轴

## 设备动画系统 (src/animation/)

基于 QGraphicsView/QGraphicsScene 的 60fps 设备动画：

| 图形项 | 类名 | 说明 |
|--------|------|------|
| 传送带 | `ConveyorBeltItem` | 滚轮纹理 + 滚动动画 |
| 板子 | `BoardItem` | PCB 矩形，可显示 Mark 点 |
| 挡板 | `StopperItem` | 可升降挡块 |
| 相机头 | `CameraHeadItem` | 跟随 CameraX/Y 移动 |
| 激光头 | `LaserHeadItem` | 跟随 LaserX/Y 移动 |
| 报警灯 | `AlarmIndicatorItem` | 闪烁红灯 |

### AnimationWidget

封装 QGraphicsView，提供：
- `zoomIn()` / `zoomOut()` — 0.3x ~ 6.0x 缩放
- `fitToWindow()` — 一键自适应窗口
- `tick()` — 驱动 motion → scene 更新链
- 鼠标滚轮缩放 + 拖拽平移

### DeviceAnimationScene

统一更新入口 `updateFromMotion()`：从 IMotionController 读取各轴位置，映射到图形项坐标，计算传送带速度增量。

## 相机策略模式 (src/camera/)

- `ICameraProvider` — 统一接口 (open/close/capture)
- `MacCameraProvider` — OpenCV `cv::VideoCapture(0)` 摄像头实时帧
- `DemoImageCameraProvider` — 从大尺寸虚拟 PCB 图片按 CameraX/Y 坐标 crop 视野窗口

## 设备状态机 (src/statemachine/)

`MachineStateMachine` 定义工业设备主状态转移表：

```
PowerOff → Initializing → Idle → WaitingBoard → LoadingBoard →
BoardArrived → StopperPositioning → ReadyForInspection →
MovingCameraToMark → CapturingImage → DetectingMark →
CalculatingOffset → ApplyingCompensation → Inspecting →
LaserMarking → UnloadingBoard → Completed → WaitingBoard (循环)

任意状态 → Alarm / EmergencyStopped → Resetting → Idle
```

每个状态定义 `onEnter()` / `onExit()` 动作，通过回调通知外部。

## 运控调试窗口 (MotionControlDialog)

独立全功能窗口，集成：
- 大尺寸设备动画区 (420px 最小高度)
- 缩放工具栏（放大/缩小/适应）
- IO 控制面板（挡板升降、进板、出板、复位）
- 4 轴控制网格（目标位置/步进量/当前位置/状态）
- 急停/复位按钮 + 报警状态指示
- 实时日志面板
- QScrollArea 包裹，支持小窗口滚动

## 设计原则

- 接口抽象 (`IMotionController`, `ITransportController`, `ICameraProvider`, `ILaserController`)
- 动画反映真实状态 — 轴位置变化驱动图形项位置
- 60fps tick 统一驱动 motion → animation → state machine 更新链
- 支持从 Demo 模式渐进演进到真实硬件对接
