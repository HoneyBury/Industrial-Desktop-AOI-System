# Industrial-Desktop-AOI-System

[![CI](https://github.com/HoneyBury/Industrial-Desktop-AOI-System/actions/workflows/ci.yml/badge.svg)](https://github.com/HoneyBury/Industrial-Desktop-AOI-System/actions/workflows/ci.yml)
[![CodeQL](https://github.com/HoneyBury/Industrial-Desktop-AOI-System/actions/workflows/codeql.yml/badge.svg)](https://github.com/HoneyBury/Industrial-Desktop-AOI-System/actions/workflows/codeql.yml)
[![Release](https://github.com/HoneyBury/Industrial-Desktop-AOI-System/actions/workflows/release.yml/badge.svg)](https://github.com/HoneyBury/Industrial-Desktop-AOI-System/actions/workflows/release.yml)

## 项目简介

`Industrial-Desktop-AOI-System` 是一个用于面试展示的桌面级工业 AOI 视觉检测与虚拟运动校准系统。
项目聚焦于工业 AOI 上位机常见能力，包括视觉标定、Mark 对位、ROI 检测、二维码读取、虚拟运动控制、AI 模型部署与企业级研发流程。

当前版本针对 MacBook M4 Pro 开发环境进行了工程化取舍：

- 使用 Mac 自带摄像头模拟工业相机
- 使用虚拟运动控制器模拟 X/Y/Z/R 轴运动平台
- 使用 Qt 6 + C++20 + CMake 作为桌面工业软件基础框架
- 使用 OpenCV、SQLite、GoogleTest、Python、YOLO/ONNX 预留后续扩展能力

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
- clang-format / clang-tidy / CodeQL

## 系统架构

项目采用分层模块化结构：

- `camera`：相机抽象层，当前支持 `ICamera` 与 `UsbCamera`
- `vision`：标定、Mark 检测、ROI 检测、读码、坐标转换
- `motion`：运动控制抽象与虚拟 X/Y/Z/R 轴实现
- `program`：检测程序定义、保存与加载
- `database`：SQLite 数据管理入口
- `ai`：ONNX 推理接口与 AI 检测结果封装
- `ui`：Qt 主界面与功能对话框
- `tests`：单元测试与集成测试
- `docs`：架构、流程、标定、AI、CI/CD 等专业文档

详细架构见 [docs/architecture.md](docs/architecture.md)。

## 功能模块

- 相机接入：Mac 摄像头模拟工业相机
- 视觉标定：棋盘格标定、像素与毫米映射、坐标转换
- Mark 对位：双 Mark 角度偏移计算、原点校正预留
- ROI 检测：阈值、轮廓、模板检测扩展点
- 运动控制：虚拟平台回零、绝对移动、相对移动、急停
- 程序管理：程序新建、编辑、保存、加载
- 数据存储：程序、检测记录、AI 检测结果落库入口
- AI 推理：YOLO 训练到 ONNX 部署的工程骨架

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

如果本机已安装 Qt 6 / OpenCV / SQLite，CMake 会自动启用对应能力；如果暂时缺失，工程会退化为可编译的骨架模式，不阻塞文档、流程和核心算法开发。

### 3. 编译

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
cmake --build --preset default
```

### 本地 Release

```bash
cmake --preset release
cmake --build --preset release
```

## 测试方式

当前已初始化：

- `test_coordinate_transformer.cpp`
- `test_virtual_motion_controller.cpp`
- `test_mark_offset.cpp`
- `test_inspection_pipeline.cpp`

覆盖范围包括：

- 像素偏移到毫米转换
- 虚拟轴绝对移动
- 虚拟轴相对移动
- 双 Mark 点角度计算
- AOI 检测主流程最小集成验证

## CI/CD

- `ci.yml`：构建、CTest、clang-tidy
- `codeql.yml`：C++ 安全扫描
- `release.yml`：Tag 触发 Release 打包

详见 [docs/ci_cd.md](docs/ci_cd.md)。

## 面试展示流程

建议展示顺序：

1. 打开主界面，介绍 AOI 上位机模块划分
2. 说明 Mac 摄像头与虚拟运动平台的工程化替代方案
3. 演示标定、Mark 对位、ROI、程序管理与 AI 部署设计
4. 展示 GoogleTest、GitHub Actions、CodeQL、分支策略与代码审查流程
5. 说明如何扩展到海康/大华相机与真实运动控制卡

详细脚本见 [docs/interview_demo_script.md](docs/interview_demo_script.md)。

## 当前限制

- 当前使用 Mac 摄像头模拟工业相机
- 当前使用虚拟轴模拟真实运动控制器
- AI 推理、数据库与视觉算法为可编译骨架，后续逐步替换为真实实现

## 后续扩展

- 海康 / 大华工业相机 SDK 接入
- 真实运动控制卡与 EtherCAT / 脉冲轴控制
- ONNX Runtime / TensorRT 推理部署
- 更完整的 AOI 检测流程、数据回放与结果追溯
- 多线程采图、异步推理、批次统计和设备健康监控

## 项目路线图

- `v0.1`：企业级项目骨架、文档、CI/CD、测试基线
- `v0.2`：相机采集、虚拟运动联动、标定流程联调
- `v0.3`：程序编辑器、Mark 对位、ROI 检测
- `v0.4`：AI 数据采集、YOLO 训练、ONNX 推理接入
- `v0.5`：检测记录存储、结果看板、Demo 优化

## 开发流程

采用 Conventional Commits 与 Git Flow 风格分支策略：

- `main`：稳定发布
- `develop`：日常集成
- `feature/*`：功能开发
- `bugfix/*`：缺陷修复
- `release/*`：发布准备
- `hotfix/*`：紧急修复

详见：

- [docs/development_workflow.md](docs/development_workflow.md)
- [docs/branch_strategy.md](docs/branch_strategy.md)
- [docs/code_review.md](docs/code_review.md)

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).

