# 系统架构总览

## 项目定位

本仓库是一个桌面级工业 AOI 软件的工程化实现，模拟真实 AOI 上位机在视觉、运动、程序管理、数据追溯、AI 部署和生产流程上的完整架构。

## 分层设计

- `UI 层`：操作员界面、程序编辑、标定对话框、Run mode 生产看板、运控调试窗口
- `应用层`：程序生命周期、8 步检测流程编排 (ProcessEngine + BoardWorkflow)、设备状态机
- `领域层`：视觉检测、运动控制、坐标转换、标定计算、Mark 对位、运输控制
- `基础设施层`：相机接入、SQLite 持久化、AI 推理、激光控制抽象、设备动画

## 核心模块

| 模块 | 路径 | 说明 |
|------|------|------|
| `animation` | `src/animation/` | QGraphicsView 设备动画：传送带、板子、挡板、相机头、激光头、报警灯，60fps tick 驱动 |
| `camera` | `src/camera/` | 相机策略模式：`ICameraProvider` 接口 + `MacCameraProvider` + `DemoImageCameraProvider` |
| `vision` | `src/vision/` | MarkDetector (轮廓+HoughCircles)、RoiDetector (OTSU+Canny)、CameraCalibrator、CodeReader |
| `calibration` | `src/calibration/` | 5 个独立标定器：CameraIntrinsic、PixelScale、Origin、LaserOffset、MarkReference |
| `coordinate` | `src/coordinate/` | 六层坐标链：CameraPixel → Undistorted → ImagePhysicalMm → Product → Machine → Laser |
| `alignment` | `src/alignment/` | MarkAlignmentSolver (单/双/多Mark最小二乘)、MarkDetector 包装器 |
| `motion` | `src/motion/` | `IMotionController` 接口 + `VirtualMotionController` 8 轴梯形加减速模拟 |
| `transport` | `src/transport/` | `ITransportController` 接口 + `VirtualTransportController` 闭环运输模拟 |
| `laser` | `src/laser/` | `ILaserController` 接口 + `VirtualLaserController` 虚拟激光（十字/二维码/文字标记） |
| `statemachine` | `src/statemachine/` | `MachineStateMachine` 工业设备主状态机：状态转移表 + 进入/退出动作 |
| `process` | `src/process/` | 8 步工业流程引擎：ProcessEngine、BoardWorkflow、IProcessStep |
| `program` | `src/program/` | ProgramModel (分层标定+ROI配置+激光参数)、ProgramManager (JSON序列化) |
| `database` | `src/database/` | DatabaseManager：SQLite3 真实实现，3 表 schema，完整 CRUD |
| `ai` | `src/ai/` | AiInferencer：OpenCV DNN ONNX → 传统CV统计特征 → 桩回退，三层推理 |
| `ui` | `src/ui/` | Qt6 桌面 HMI：MainWindow (Editor/Run 双模式)、MotionControlDialog、RunModeWidget、10+ 对话框 |

## 设计原则

- 硬件能力通过接口抽象 (`IMotionController`、`ITransportController`、`ILaserController`、`ICameraProvider`)
- 关键算法脱离 GUI 独立可测
- 缺少工业硬件或 SDK 时工程可退化运行
- 动画反映真实状态：轴位置变化 → 图形项移动 → 状态机转移，统一由 60fps tick 驱动
- 支持从原型到真实工业设备的渐进演进
