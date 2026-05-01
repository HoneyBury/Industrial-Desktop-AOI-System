# 工业校正与坐标链路后续规划

## 1. 目标

本项目不是普通的 OpenCV Demo，而是一个用于演示真实工业 AOI / 镭雕上位机核心能力的桌面原型。

后续规划必须围绕以下工业能力展开：

- 多坐标系统
- 分层校正
- 运动控制与视觉协同
- Mark 每板实时对位
- 相机与执行器偏移补偿
- 程序 / 配方管理
- 状态机驱动流程
- 可替换真实硬件接口

当前项目没有真实硬件，因此所有模块都必须同时满足两件事：

1. 业务语义贴近真实工业设备  
2. 能在虚拟演示模式下完整跑通  

---

## 2. 核心工业认知

工业定位不是一个“校正完成”的单点动作，而是一条分层链路：

```text
机械粗定位
→ 产品原点建立
→ 图像像素坐标与实际坐标换算
→ 相机与激光执行器偏移校正
→ Mark 点实时修正每一块板的位置偏差
→ 得到最终检测 / 镭射坐标
```

必须严格区分以下四个概念：

### 2.1 原点校正 Origin Calibration

解决：

- 当前产品程序的产品坐标原点在哪里
- Product Coordinate 与 Machine Coordinate 如何建立基准关系

关键词：

- 建程序
- 换产品
- 产品坐标系
- 机械坐标系
- 基准点

### 2.2 像素到机械坐标转换 Pixel to Machine Mapping

解决：

- 图像里偏了多少像素，实际相当于多少毫米
- 图像点击点如何换算成运动轴相对移动量

关键词：

- pixel/mm
- 图像中心
- 像素偏移
- 机械移动量

### 2.3 激光偏移校正 Laser Offset Calibration

解决：

- 相机中心 Camera TCP 和激光出光点 Laser TCP 不是同一个位置时的补偿

关键词：

- Camera TCP
- Laser TCP
- 工具偏移
- 十字打标

### 2.4 Mark 点校正 Mark Alignment

解决：

- 当前这块板相对标准板偏到哪里了
- 每块板进板后如何实时修正平移和旋转误差

关键词：

- 每块板
- dx / dy / theta
- 单 Mark
- 双 Mark
- 刚体变换

总结：

- 原点校正 = 建地图
- 像素转换 = 尺子
- 激光偏移 = 工具补偿
- Mark 对位 = 每块产品实时纠偏

---

## 3. 新的模块划分

建议按“坐标链 + 校正链 + 业务执行链”重新梳理模块。

### 3.1 设备与执行层

- `VirtualMotionController`
- `VirtualTransportController`
- `VirtualMotionSystem`
- `VirtualCameraDevice`
- `VirtualLaserController`

职责：

- 提供统一的虚拟硬件行为
- 为后续真实硬件接口保留替换边界

### 3.2 坐标与变换层

- `CoordinateTransformer`
- `PixelToMachineCalibrationModule`
- `CoordinateMappingProfile`

职责：

- 管理 Pixel / CameraLocalMm / Machine / Product / Laser 各层转换
- 统一提供所有业务动作需要的坐标换算 API

### 3.3 校正层

- `OriginCalibrationModule`
- `LaserOffsetCalibrationModule`
- `MarkAlignmentModule`
- `CameraIntrinsicCalibrationModule`
- `PixelScaleCalibrationModule`

职责：

- 各自维护独立的输入、输出、保存字段与执行逻辑
- 不把不同校正逻辑混进一个 UI 或一个步骤里

### 3.4 视觉识别层

- `MarkDetector`
- `LaserCrossDetector`
- `CodeReader`
- `RoiDetector`

职责：

- 提供算法检测能力
- 为校正与检测业务提供坐标输入

### 3.5 业务编排层

- `ProcessEngine`
- `BoardWorkflow`
- `WorkflowContext`
- `MachineStateMachine`

职责：

- 用状态机和流程引擎组织“进板 → 校正 → Mark → 检测 / 镭射 → 出板”

### 3.6 UI 与交互层

- `OriginCalibDialog`
- `LaserOffsetCalibDialog`
- `CalibrationJogPanel`
- `CameraPreviewWidget`
- 编辑界面右侧对象定位控制区

职责：

- 只负责交互和状态展示
- 校正计算与坐标换算必须调用底层模块，不允许写死在 UI 里

---

## 4. 各校正模块职责

### 4.1 OriginCalibrationModule

职责：

- 手动移动相机到产品基准点
- 设置产品原点
- 记录原点对应的机械坐标
- 保存到程序配置
- 提供 Product ↔ Machine 的基础基准关系

输入：

- 当前程序板尺寸
- 当前机械坐标
- 当前实时相机画面
- 用户选定的产品基准点定义，如右下角

输出：

- `originCalibration.machineReferencePose`
- `originCalibration.imageReferencePixel`
- `runtimeSummary.originCorrectedPose`

调用方：

- 整板扫描规划
- 编辑界面对象定位
- ROI / LaserPoint / CodePoint 坐标换算

### 4.2 PixelToMachineCalibrationModule

职责：

- 保存 `pixelToMmX / pixelToMmY`
- 支持图像中心偏移换算成机械移动量
- 支持 Pixel → CameraLocalMm
- 支持 CameraLocalMm → Machine
- 为后续仿射矩阵 / Homography 预留升级入口

输入：

- 图像分辨率
- FOV 物理尺寸
- 像素比例参数或标定板结果

输出：

- `pixelToMmX`
- `pixelToMmY`
- 图像点点击后的机械移动增量

调用方：

- 原点校正点击移动
- 激光偏移点击十字中心
- 后续图像点击定位

### 4.3 LaserOffsetCalibrationModule

职责：

- 控制 `VirtualLaserController` 在板上生成十字
- 在相机图像中显示十字
- 支持用户点击十字中心
- 支持 `LaserCrossDetector` 自动识别十字中心
- 根据像素到机械坐标转换结果计算 `cameraToLaserOffset`
- 保存 `laserOffsetX / laserOffsetY`
- 让后续激光执行自动应用偏移

输入：

- 当前机械坐标
- 十字中心像素坐标
- PixelToMachineMapping 参数

输出：

- `laserOffsetCalibration.cameraToLaserDxMm`
- `laserOffsetCalibration.cameraToLaserDyMm`

调用方：

- `PreLaserStep`
- `LaserExecuteStep`
- 虚拟镭射预览与结果生成

### 4.4 MarkAlignmentModule

职责：

- 记录标准 Mark 点
- 支持每块板识别当前 Mark 点
- 支持单 Mark / 双 Mark
- 计算 `dx / dy / theta`
- 输出刚体变换矩阵
- 将该变换应用到 ROI、LaserPoint、CodePoint、InspectionPoint
- 判断偏移是否超限
- 生成报警

输入：

- 标准 Mark 数据
- 当前板 Mark 检测结果

输出：

- `translation dx / dy`
- `rotation theta`
- `rigid transform`
- 偏移是否超限

调用方：

- ROI 检测
- AOI 点位
- 镭射点
- 二维码点
- 整板扫描点位

---

## 5. CoordinateTransformer 设计

`CoordinateTransformer` 必须从“工具类”升级为完整的坐标链中枢。

### 5.1 目标链路

```text
Pixel(u, v)
→ PixelOffset(du, dv)
→ CameraLocalMm(dx, dy)
→ Machine(x, y)
→ Product(x, y)
→ ProductCorrected(x, y)
→ FinalLaserMachine(x, y)
```

### 5.2 必备能力

- `pixelToCameraLocal(pixelPoint)`
- `cameraLocalToMachine(cameraLocalMm, currentMachinePose)`
- `machineToProduct(machinePoint, originPose)`
- `productToMachine(productPoint, originPose)`
- `applyMarkTransform(productPoint, markTransform)`
- `applyLaserOffset(machinePose, laserOffset)`
- `imageClickToMoveDelta(pixelPoint, imageCenter)`
- `moveCameraToImagePoint(pixelPoint)`
- `targetProductPointToCameraPosition(productPoint)`
- `targetProductPointToLaserPosition(productPoint)`

### 5.3 建议的数据结构

- `PixelPoint`
- `MillimeterPoint`
- `MechanicalPose`
- `OriginReference`
- `PixelScaleMapping`
- `MarkTransform2D`
- `LaserToolOffset`
- `CoordinateMappingProfile`

### 5.4 设计原则

- 不把标定逻辑写死在 UI
- 不让 `MainWindow` 直接拼接坐标计算
- 同一条坐标链对编辑、校正、扫描、镭射、检测统一可复用

---

## 6. 配置文件应保存的参数

建议按“程序级参数”和“设备级参数”分层保存。

### 6.1 程序级参数

- `boardDefinition.boardLengthMm`
- `boardDefinition.boardWidthMm`
- `boardDefinition.railWidthMm`
- `scanRecipe.fovWidthMm`
- `scanRecipe.fovHeightMm`
- `originCalibration.calibrated`
- `originCalibration.machineReferencePose`
- `originCalibration.imageReferencePixel`
- `pixelScaleCalibration.calibrated`
- `pixelScaleCalibration.pixelToMillimeterX`
- `pixelScaleCalibration.pixelToMillimeterY`
- `laserOffsetCalibration.calibrated`
- `laserOffsetCalibration.cameraToLaserDxMm`
- `laserOffsetCalibration.cameraToLaserDyMm`
- `markReferences`
- `marks`
- `rois`
- `laserPointTasks`
- `roiDetectorConfigs`

### 6.2 运行时摘要

- `runtimeSummary.hasOriginCalibration`
- `runtimeSummary.originCorrectedPose`
- `runtimeSummary.hasLaserOffsetCalibration`
- `runtimeSummary.laserOffsetDxMm`
- `runtimeSummary.laserOffsetDyMm`
- `runtimeSummary.hasMarkCalibration`
- `runtimeSummary.markCalibrationOffsetXmm`
- `runtimeSummary.markCalibrationOffsetYmm`
- `runtimeSummary.markCalibrationRotationDegrees`
- `runtimeSummary.wholeBoardImagePath`
- `runtimeSummary.lastBoardScanSummary`

### 6.3 设备级参数

- 相机分辨率
- 虚拟整板图路径
- 默认挡板参考位定义
- 虚拟 FOV 分辨率
- 运动默认速度
- 点动默认步长

---

## 7. UI 中应具备的校正窗口

### 7.1 原点校正窗口

用于建立 Product ↔ Machine 基准关系。

### 7.2 镭射偏移校正窗口

用于建立 Camera TCP ↔ Laser TCP 偏移补偿。

### 7.3 相机像素换算调试窗口

不一定必须对操作员开放，但至少在工程模式中存在，用于：

- 显示 pixel/mm 参数
- 测试图像点击后相机相对移动是否准确

### 7.4 Mark 参考定义 / Mark 对位调试窗口

用于：

- 标准 Mark 录入
- 当前 Mark 检测预览
- 偏移 / 旋转结果查看

说明：

- 现有 `MarkOffsetDialog` 不应继续作为主线校正窗口
- 它应被替换为“Mark 参考定义”和“Mark 每板对位结果”两个更工业语义化的功能

---

## 8. 每个校正窗口的操作流程

### 8.1 原点校正窗口流程

1. 进板并挡板定位
2. 打开原点校正窗口
3. 左侧显示实时虚拟相机画面
4. 右侧通过上下左右按钮点动相机
5. 让中心十字对准产品右下角或定义基准点
6. 点击 `设置原点`
7. 系统记录当前机械坐标
8. 系统换算并保存逻辑产品原点
9. 点击 `应用原点`

### 8.2 镭射偏移校正窗口流程

1. 让虚拟激光在板上打十字
2. 相机移动到十字附近
3. 左侧显示实时虚拟相机画面
4. 用户点动相机，让十字进入中心区域
5. 用户点击十字中心，或算法自动识别十字中心
6. 点击 `记录相机参考点`
7. 点击 `记录镭射参考点`
8. 系统计算 `cameraToLaserOffset`
9. 点击 `应用镭射偏移`

### 8.3 Mark 参考定义 / 对位流程

1. 新建程序时记录标准 Mark1 / Mark2
2. 每块板进板后拍摄 Mark
3. 识别当前 Mark 点
4. 计算 `dx / dy / theta`
5. 生成刚体变换
6. 应用于 ROI / LaserPoint / CodePoint / InspectionPoint

---

## 9. 每个校正流程的输入、输出、保存数据

### 9.1 原点校正

输入：

- 当前实时图像
- 当前机械坐标
- 板尺寸
- 用户选择的基准角点

输出：

- Product(0, 0) ↔ Machine(x, y)

保存：

- `originCalibration.machineReferencePose`
- `originCalibration.imageReferencePixel`
- `runtimeSummary.originCorrectedPose`

### 9.2 像素坐标换算

输入：

- 图像中心
- 点击点像素
- `pixelToMmX / pixelToMmY`

输出：

- `dx_mm / dy_mm`

保存：

- `pixelScaleCalibration.pixelToMillimeterX`
- `pixelScaleCalibration.pixelToMillimeterY`

### 9.3 激光偏移校正

输入：

- 十字中心像素坐标
- 当前机械坐标
- PixelToMachineMapping

输出：

- `cameraToLaserDxMm / DyMm`

保存：

- `laserOffsetCalibration.cameraToLaserDxMm`
- `laserOffsetCalibration.cameraToLaserDyMm`
- `runtimeSummary.laserOffsetDxMm`
- `runtimeSummary.laserOffsetDyMm`

### 9.4 Mark 对位

输入：

- 标准 Mark
- 当前板 Mark 检测结果

输出：

- `dx`
- `dy`
- `theta`
- `rigid transform`

保存：

- 标准 Mark 参考信息
- 运行时对位结果
- 超限报警记录

---

## 10. Mark 点校正对下游对象的影响

Mark 对位结果必须作用于所有“产品坐标上的业务点”。

### 10.1 ROI

- 每块板检测前，先把标准 ROI 用刚体变换修正到当前板位置

### 10.2 AOI 检测点

- AOI 特征点 / 取样点必须基于修正后坐标执行

### 10.3 镭射点

- 标准镭射点先做 Mark 刚体变换，再转机械坐标，再叠加激光偏移

### 10.4 二维码点

- 二维码生成或验证位置同样必须使用修正后的点位

### 10.5 整板扫描点

- 若要模拟更真实的生产场景，整板扫描中心点也应支持基于 Mark 结果微调

---

## 11. 虚拟演示模式下如何模拟

### 11.1 原点校正模拟

- 使用 `demoimage/board.png`
- 用户在实时图像中把中心十字对到右下角
- 记录当前虚拟机械坐标

### 11.2 像素换算模拟

- 使用 `pixelToMmX / pixelToMmY`
- 或使用 FOV 物理尺寸和图像分辨率反推出默认比例

### 11.3 激光偏移校正模拟

- `VirtualLaserController` 在整板图上绘制十字
- `VirtualCameraDevice` 根据相机位置裁图
- 用户点击十字中心
- 系统计算虚拟 `laserOffset`

### 11.4 Mark 对位模拟

- 在 demo 板图中预制两个 Mark
- 引入每块板不同的平移 / 旋转扰动
- `MarkDetector` 识别当前 Mark
- `MarkAlignmentModule` 计算 `dx / dy / theta`
- 更新所有 ROI、LaserPoint、CodePoint 位置

### 11.5 状态机模拟

```text
进板
→ 挡板定位
→ 原点 / 校正
→ Mark 拍摄
→ 偏移计算
→ 补偿执行
→ 检测
→ 镭射 / 二维码生成
→ 出板
```

---

## 12. 后续开发优先级

### P0

1. 完成 `OriginCalibrationModule`
2. 完成 `PixelToMachineCalibrationModule` 与 `CoordinateTransformer` 收口
3. 完成交互式 `LaserOffsetCalibrationModule`
4. 移除 `MarkOffsetDialog` 主线角色
5. 编辑界面 `定位到选中` 统一接入虚拟相机和虚拟运控

### P1

6. 完成 `MarkAlignmentModule` 的标准参考与每板对位闭环
7. 把 Mark 变换统一作用到 ROI、LaserPoint、CodePoint、InspectionPoint
8. 增加 `LaserCrossDetector`
9. 增加像素到机械移动调试窗口

### P2

10. 清理 `ProgramModel` 过渡字段
11. 抽出校正窗口通用控制组件
12. 为真实工业相机 / 控制卡 / PLC / 激光卡保留 provider 接口实现位

---

## 13. 分阶段验收标准

### 阶段 A：原点与像素链路

验收标准：

- 能在原点窗口中点动相机
- 左侧实时画面持续变化
- 设置原点后能保存 Product ↔ Machine 基准
- 编辑界面选中对象后可正确移动到对应位置

### 阶段 B：激光偏移链路

验收标准：

- 能在板上看到虚拟十字
- 能点击或识别十字中心
- 能计算并保存 `cameraToLaserDxMm / DyMm`
- 后续虚拟镭射执行自动带上偏移

### 阶段 C：Mark 每板对位

验收标准：

- 每块板进板后都能识别 Mark
- 能输出 `dx / dy / theta`
- ROI、镭射点、二维码点、检测点都能随 Mark 结果修正
- 超限时能报警或阻止继续生产

### 阶段 D：完整演示闭环

验收标准：

- 进板
- 挡板定位
- 原点 / 激光偏移 / Mark 对位
- 检测 / 镭射 / 二维码
- 出板

以上全过程可在虚拟模式下一次跑通，并且界面状态、日志、动画、相机画面保持一致。
