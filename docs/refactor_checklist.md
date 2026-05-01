# 项目重构清单

本文档记录项目从演示型 AOI 原型逐步重构为真实工业镭雕/AOI 设备工程结构的全过程。

## 一、重构目标

1. 进板与粗定位
2. 原点校正
3. 激光偏移校正
4. 双 Mark / 多 Mark 定位
5. ROI / AI 检测
6. 坐标补偿
7. 激光执行与 Post-laser 验证
8. 最终 OK / NG 输出

## 二、各阶段状态

### Phase 1：配方模型与坐标系统 ✅ 已完成

- `ProgramModel` 重构：分层标定数据、ROI 检测器配置、运行摘要、激光参数
- `ProgramManager` 重构：JSON schema 升级，保存/加载工业字段
- `coordinate` 模块独立：六层坐标链 (CameraPixel → Undistorted → ImagePhysicalMm → Product → Machine → Laser)
- 涉及文件：`src/program/*`、`src/coordinate/*`、`config/default_program.json`

### Phase 2：标定系统拆层 ✅ 已完成

- `calibration` 模块：5 个独立标定器
  - `CameraIntrinsicCalibrator`：真实 OpenCV `cv::calibrateCamera()` 棋盘格标定，支持自动回退
  - `PixelScaleCalibrator`：物理距离/像素距离比值计算
  - `OriginCalibrator`：像素偏移→mm→机械位姿修正
  - `LaserOffsetCalibrator`：Camera-to-Laser 固定偏移
  - `MarkReferenceCalibrator`：Mark 点→MarkReferenceRecord 转换
- 标定对话框只负责采图和参数编辑，核心计算已迁移到 `calibration` 模块

### Phase 3：Mark 定位与检测器体系 ✅ 已完成

- `alignment` 模块：
  - `MarkAlignmentSolver`：单 Mark 平移 / 双 Mark 刚体 / 多 Mark 最小二乘 (Kabsch-Umeyama)
  - `MarkDetector`：视觉检测包装器
- `vision` 检测器体系：
  - `MarkDetector`：真实 OpenCV 实现（自适应阈值轮廓 + HoughCircles 回退）
  - `RoiDetector`：真实 OpenCV 实现（OTSU 阈值 + Canny 边缘）
- `ai` 模块：
  - `AiInferencer`：三层推理架构（OpenCV DNN ONNX → 传统CV统计特征 → 桩回退）

### Phase 4：流程引擎 ✅ 已完成

- `process` 模块：8 步工业流程
  - LoadBoard → RoughPosition → ImageCapture → MarkAlign → DefectInspect → PreLaser → LaserExecute → PostLaserVerify
- `ProcessEngine`：支持 `runBoard()`、`runAllBoards()`、`requestCancel()`
- `BoardWorkflow`：步骤编排，支持 `ContinueWorkflow`/`StopWorkflow` 失败策略
- `laser` 模块：
  - `ILaserController`：抽象接口 (`isReady`、`executeMark`、`emergencyStop`、`resetEmergencyStop`)
  - `VirtualLaserController`：虚拟实现，带状态追踪

### Phase 5：工业 HMI 收口 ✅ 已完成

- Run mode 界面 (`RunModeWidget`)：左侧锁定预览、右侧生产看板 (OK/NG/板号/总数/生产日志)
- AppMode 切换：Editor ↔ Run 通过 QStackedWidget
- 工作流定时器驱动：600ms 间隔推进步骤
- 所有对话框已接入 MainWindow：
  - CameraCalibDialog、OriginCalibDialog、MarkOffsetDialog、MotionControlDialog
  - SettingsDialog、ProgramEditDialog、LogWindow
  - DataCollectDialog（数据集采集）、MarkEditDialog（完整 Mark 点编辑器）

## 三、后续扩展方向

- 连接真实激光硬件（替换 VirtualLaserController）
- 连接真实运动控制卡（替换 VirtualMotionController）
- 训练并部署真实 ONNX 缺陷检测模型
- MES/SPC 系统对接
- 多相机支持
