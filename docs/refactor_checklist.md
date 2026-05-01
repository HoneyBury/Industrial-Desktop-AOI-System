# 项目重构清单

本文档用于把当前项目从演示型 AOI 原型，逐步重构为符合真实工业镭雕 / AOI 设备流程的工程结构。

重构依据：

- `.ai/00_project_snapshot.md`
- `.ai/01_architecture_and_modules.md`
- `.ai/03_engineering_rules.md`
- `.ai/04_current_progress_and_next_steps.md`

## 一、重构目标

目标不是继续堆叠 UI 功能，而是建立一套能支撑真实工业流程的架构：

1. 进板与粗定位
2. 原点校正
3. 激光偏移校正
4. 双 Mark 定位
5. ROI / AI 检测
6. 坐标补偿
7. 最终执行与 OK / NG 输出

## 二、整体改造阶段

### Phase 1：配方模型与坐标系统

目标：

- 重构 `ProgramModel`
- 重构 `ProgramManager`
- 建立独立 `coordinate` 模块

涉及文件：

- `src/program/ProgramModel.h`
- `src/program/ProgramManager.h`
- `src/program/ProgramManager.cpp`
- `src/coordinate/*`
- `src/vision/CoordinateTransformer.h`
- `src/vision/CoordinateTransformer.cpp`
- `config/default_program.json`
- `tests/unit/test_program_manager.cpp`
- `tests/unit/test_coordinate_transformer.cpp`
- `tests/unit/test_mark_offset.cpp`

验收标准：

- Program 配方包含分层标定数据
- ROI 能绑定检测器配置
- 坐标链至少支持：
  - `CameraPixel`
  - `UndistortedPixel`
  - `ImagePhysicalMm`
  - `Product`
  - `Machine`
  - `Laser`
- `cmake --build --preset default --parallel` 通过
- `ctest --preset default` 通过

### Phase 2：标定系统拆层

目标：

- 建立 `calibration` 模块
- 拆分相机标定、像素转 mm、原点、激光偏移、Mark 基准标定

涉及文件：

- `src/calibration/*`
- `src/ui/CameraCalibDialog.*`
- `src/ui/OriginCalibDialog.*`
- `src/ui/MarkOffsetDialog.*`
- `src/ui/CalibrationCaptureWidget.*`
- `src/program/ProgramModel.h`
- `src/program/ProgramManager.cpp`

验收标准：

- 每类标定数据独立存储
- 标定对话框只负责采图和参数编辑
- 核心计算从 UI 迁移到 `calibration`

### Phase 3：Mark 定位与检测器体系

目标：

- 建立 `alignment` 模块
- 拆分 `vision` 检测器体系

涉及文件：

- `src/alignment/*`
- `src/vision/MarkDetector.*`
- `src/vision/RoiDetector.*`
- `src/ai/AiInferencer.*`
- `src/program/ProgramModel.h`

验收标准：

- 支持单 Mark 平移
- 支持双 Mark 平移 + 旋转
- ROI 绑定多种检测器
- 输出统一检测结果结构

### Phase 4：流程引擎

目标：

- 建立 `process` 模块
- 将工业流程从 `MainWindow` 中抽离

涉及文件：

- `src/process/*`
- `src/ui/MainWindow.*`
- `src/motion/*`
- `src/camera/*`
- `src/program/*`

验收标准：

- 流程状态机独立存在
- UI 仅发命令和显示状态
- 支持：
  - `LOAD_BOARD`
  - `ROUGH_POSITION`
  - `ORIGIN_CALIBRATION`
  - `LASER_OFFSET_CALIBRATION`
  - `FIND_MARK`
  - `CALCULATE_ALIGNMENT`
  - `APPLY_COMPENSATION`
  - `INSPECT`
  - `EXECUTE`
  - `OUTPUT_RESULT`

### Phase 5：工业 HMI 收口

目标：

- 将现有主界面收口成工业 HMI
- 展示流程、坐标、补偿、检测和执行结果

涉及文件：

- `src/ui/MainWindow.*`
- 各校正与编辑对话框

验收标准：

- 主界面显示坐标链数据
- 显示 Mark / ROI / 偏移 / OK-NG
- 显示流程状态和执行结果

## 三、当前代码必须修改的重点

### 1. `ProgramModel`

当前问题：

- 标定数据还是一个混合结构
- 缺少激光偏移、Mark 基准、ROI 检测器配置
- 运行摘要与长期工艺数据边界不清晰

必须修改：

- 引入分层标定结构
- 引入 ROI 检测器配置结构
- 引入运行摘要结构

### 2. `ProgramManager`

当前问题：

- 配方 schema 还是 demo 结构
- 手写正则解析脆弱
- 没有围绕工业配方设计

必须修改：

- 升级默认配方 schema
- 保存 / 加载新的工业字段
- 保持对现有 UI 的过渡兼容

### 3. `CoordinateTransformer`

当前问题：

- 只支持像素到毫米和简单姿态计算
- 没有显式坐标层级

必须修改：

- 拆出独立 `src/coordinate`
- 建立六层坐标系统
- 支持刚体变换与链式转换

### 4. `MainWindow`

当前问题：

- 承担了过多流程和业务逻辑
- 校正、模板、补偿都直接写在 UI 里

后续必须修改：

- 逐步改成 HMI 层
- 把流程、标定、对位、转换迁走

## 四、当前推荐实施顺序

1. 完成 `ProgramModel / ProgramManager` 重构
2. 完成 `coordinate` 模块独立
3. 再做 `calibration`
4. 再做 `alignment`
5. 最后落 `process`

## 五、本轮落地范围

本轮要求完成：

1. 新增本清单文档
2. 完成 Phase 1 的前两项：
   - `ProgramModel / ProgramManager`
   - `coordinate` 模块
3. 同步更新默认配置与单元测试
