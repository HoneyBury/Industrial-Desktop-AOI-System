# Industrial-Desktop-AOI-System

[![CI](https://github.com/HoneyBury/Industrial-Desktop-AOI-System/actions/workflows/ci.yml/badge.svg)](https://github.com/HoneyBury/Industrial-Desktop-AOI-System/actions/workflows/ci.yml)
[![Release](https://github.com/HoneyBury/Industrial-Desktop-AOI-System/actions/workflows/release.yml/badge.svg)](https://github.com/HoneyBury/Industrial-Desktop-AOI-System/actions/workflows/release.yml)

## 项目简介

`Industrial-Desktop-AOI-System` 是一个用于面试展示的桌面级工业 AOI 视觉检测与虚拟运动校准系统。
项目强调“工业 AOI、上位机、视觉标定、运动控制、AI 部署、企业级开发流程”六个关键词，目标不是一次性实现完整业务，而是先建立一套专业、清晰、可扩展、可展示的工程骨架。

当前版本针对本地演示环境做了工程化替代：

- 使用虚拟整板相机替代工业相机与系统摄像头
- 使用虚拟运动控制器模拟 X/Y/Z/R 四轴平台与运输挡板
- 使用 Qt 6 + C++20 + CMake 搭建桌面工业软件基础框架
- 使用 OpenCV、SQLite、GoogleTest、Python、YOLO/ONNX 预留后续算法与部署扩展

## 技术栈

- C++20
- Qt 6
- OpenCV
- CMake + Ninja
- SQLite
- GoogleTest
- Python 3.12
- YOLO / ONNX
- GitHub Actions
- clang-format / clang-tidy

## 系统架构

项目采用分层模块化结构，便于后续从”可展示 Demo”平滑过渡到”可继续演进的工业软件原型”：

- `camera`：相机抽象层，当前提供 `ICamera`、`UsbCamera` 与 `VirtualCameraDevice`
- `vision`：标定、Mark 检测、ROI 检测、读码、坐标转换
- `calibration`：校正模块体系（`OriginCalibrationModule`、`LaserOffsetCalibrationModule`、`PixelToMachineCalibrationModule` 等），独立无 UI 依赖
- `coordinate`：6 层工业坐标链（Pixel→UndistortedPixel→ImagePhysicalMm→Product→Machine→Laser），含完全变换链路
- `motion`：运动控制抽象、虚拟 X/Y/Z/R 轴与统一虚拟运控门面 `VirtualMotionSystem`
- `transport`：进板/出板/挡板运输状态机
- `alignment`：Mark 刚体变换求解、多模板匹配检测
- `program`：检测程序定义、保存与加载
- `database`：SQLite 数据管理入口
- `ai`：ONNX 推理接口与 AI 检测结果封装
- `ui`：Qt 主界面与功能对话框，包含可复用的 `CameraPreviewWidget`、`CalibrationJogPanel` 等校正交互组件
- `tests`：单元测试与集成测试
- `docs`：架构、流程、标定、AI、CI/CD 等专业文档

详细设计见 [docs/architecture.md](docs/architecture.md)。

## 功能模块

- 虚拟相机：基于 `demoimage/board.png` 按当前机械坐标实时裁切 FOV，模拟”相机随平台运动观察整板”的效果
- 视觉标定：支持棋盘格标定、像素与毫米映射、坐标转换
- 校正模块体系：`OriginCalibrationModule`（参考位→逻辑原点换算）、`LaserOffsetCalibrationModule`（Camera TCP→Laser TCP 偏移补偿）、`PixelToMachineCalibrationModule`（像素比例管理与 FOV 推算）
- 坐标变换链路：`CoordinateTransformer` 提供完整的 6 层坐标链与 `machineToProduct`、`applyLaserOffset`、`imageClickToMoveDelta`、`applyMarkTransform` 等工业坐标方法
- Mark 对位：支持单 Mark 平移、双 Mark 刚体对齐、多 Mark 最小二乘拟合
- 原点/运输语义：挡板位于工位右下侧，界面校正以挡板参考角点操作，内部换算为整板逻辑原点
- ROI 检测：预留阈值、轮廓、模板检测扩展点
- 运动控制：支持回零、绝对移动、相对移动、急停、进板/出板/挡板联动
- 程序管理：支持程序新建、编辑、保存、加载
- 数据存储：预留程序、检测记录、AI 检测结果落库入口
- AI 推理：提供 YOLO 训练到 ONNX 部署的工程骨架

## 虚拟整板相机

当前项目默认不访问 Mac 摄像头，而是使用虚拟整板相机：

- 画面来源：`demoimage/board.png`
- 取景方式：根据当前 `CameraX/CameraY/Z/R` 与板尺寸、FOV 尺寸、运输状态实时裁图
- 运行效果：相机移动时，主界面预览、运行界面预览、整板扫描、标定对话框中的画面都会同步变化
- 坐标语义：用户在工位右下挡板参考位操作，系统内部保存为整板逻辑原点，保证扫描规划和程序坐标仍保持一致

## 快速开始

### 1. 克隆仓库

```bash
git clone git@github.com:HoneyBury/Industrial-Desktop-AOI-System.git
cd Industrial-Desktop-AOI-System
```

### 2. 配置工程

```bash
cmake --preset default
```

如果本机安装了 Qt 6 / OpenCV / SQLite，CMake 会自动启用对应能力；如果本机暂时缺少这些依赖，工程会自动退化为“可编译骨架模式”，不阻塞文档、流程、核心算法和测试体系建设。

### 3. 编译工程

```bash
cmake --build --preset default
```

### 4. 运行测试

```bash
ctest --preset default
```

## 构建方式

### 本地 Debug

```bash
cmake --preset default
cmake --build --preset default --parallel
```

### 本地 Release

```bash
cmake --preset release
cmake --build --preset release --parallel
```

## 测试方式

当前已包含 `74` 项测试，全部通过，覆盖文件包括：

- `test_calibration_modules.cpp` — OriginCalibrationModule、LaserOffsetCalibrationModule、PixelToMachineCalibrationModule、像素比例、Mark 参考、棋盘格标定等完整校正模块测试
- `test_coordinate_transformer.cpp` — 像素到毫米、产品到机械、整像素到激光链、刚体变换、machineToProduct 逆变换、applyLaserOffset/applyMarkTransform/imageClickToMoveDelta
- `test_mark_alignment.cpp` — 单 Mark 平移、双 Mark 刚体、多 Mark 最小二乘拟合
- `test_virtual_motion_controller.cpp` — 虚拟轴绝对/相对移动、回零、急停
- `test_virtual_camera_device.cpp` — 整板相机随机械坐标切换 FOV
- `test_mark_offset.cpp` — 双 Mark 点角度计算
- `test_laser_and_steps.cpp` — 激光控制、运输、流程步骤
- `test_process_engine.cpp` — 流程引擎与 8 步骤队列
- `test_program_manager.cpp` — 程序创建、保存、加载、校正字段持久化
- `test_inspection_pipeline.cpp` — AOI 主流程的最小集成验证

## CI/CD

- `ci.yml`：构建、CTest、clang-tidy
- `release.yml`：Tag 触发 Release 打包

详见 [docs/ci_cd.md](docs/ci_cd.md)。

## 面试展示流程

推荐展示顺序如下：

1. 打开主界面，介绍 AOI 上位机模块划分
2. 说明虚拟整板相机与虚拟运动平台的工程化替代方案
3. 演示标定、Mark 对位、ROI、程序管理与 AI 部署设计
4. 展示 GoogleTest、GitHub Actions、分支策略与代码审查流程
5. 说明未来如何扩展到海康/大华相机与真实运动控制卡

详细讲解脚本见 [docs/interview_demo_script.md](docs/interview_demo_script.md)。

## 当前限制

- 当前使用虚拟整板相机模拟工业相机
- 当前使用虚拟轴模拟真实运动控制器
- AI 推理、数据库与视觉算法目前以可编译骨架和最小实现为主

## 后续扩展

- 海康 / 大华工业相机 SDK 接入
- 真实运动控制卡与 EtherCAT / 脉冲轴控制
- ONNX Runtime / TensorRT 推理部署
- 更完整的 AOI 检测流程、数据回放与结果追溯
- 多线程采图、异步推理、批次统计和设备健康监控

## 项目路线图

- `v0.1`：企业级项目骨架、文档、CI/CD、测试基线
- `v0.2`：虚拟整板相机、虚拟运动联动、标定流程联调
- `v0.3`：程序编辑器、Mark 对位、ROI 检测
- `v0.4`：AI 数据采集、YOLO 训练、ONNX 推理接入
- `v0.5`：检测记录存储、结果看板、Demo 优化
- `v0.6`（当前）：校正模块体系重构 — OriginCalibrationModule、LaserOffsetCalibrationModule、PixelToMachineCalibrationModule、CoordinateTransformer 工业坐标链补全

## 开发流程

项目采用 Conventional Commits 与 Git Flow 风格分支策略：

- `main`：稳定发布
- `develop`：日常集成
- `feature/*`：功能开发
- `bugfix/*`：缺陷修复
- `release/*`：发布准备
- `hotfix/*`：紧急修复

详细规范见：

- [docs/development_workflow.md](docs/development_workflow.md)
- [docs/branch_strategy.md](docs/branch_strategy.md)
- [docs/code_review.md](docs/code_review.md)

## 文档与注释约定

- 面向团队阅读的说明文档默认使用中文
- 关键设计说明、流程说明、接口说明优先使用中文表达
- 代码注释只在必要时添加，并优先使用简洁中文说明意图，而不是重复代码表面含义

## 许可证

本项目采用 MIT License，详见 [LICENSE](LICENSE)。
