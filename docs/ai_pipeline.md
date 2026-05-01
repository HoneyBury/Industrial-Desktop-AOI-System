# AI 流水线设计

## 推理架构

`AiInferencer` 已从占位桩升级为三层推理架构，根据运行环境自动选择：

### 第一层：OpenCV DNN (ONNX 模型)

- 条件：ONNX 模型文件存在 + OpenCV DNN 模块可用
- 流程：`cv::dnn::readNetFromONNX()` → `blobFromImage()` 预处理 (224×224, ImageNet 均值归一化) → `net.forward()`
- 输出解析：支持二分类 `[ok_score, ng_score]` 和通用多分类 max-score
- 失败时自动降级到第二层

### 第二层：传统 CV 统计特征

- 条件：无 ONNX 模型或 DNN 推理失败
- 特征提取：
  - 灰度均值与标准差 (`cv::meanStdDev`)
  - 边缘密度 (Canny 边缘像素占比)
  - 暗区比例 (OTSU 二值化反转)
  - 直方图展宽 (IQR 近似)
- 启发式评分：4 项特征加权 → 异常分数 → OK/NG 判定

### 第三层：桩回退

- 条件：无 OpenCV
- 行为：返回固定 `{label: "ok", confidence: 0.98}`

## AiDetection 结构

```cpp
struct AiDetection {
  std::string label;       // "ok" / "ng"
  double confidence;       // 0.0 - 1.0
  double bboxX, bboxY;     // 缺陷边界框 (预留)
  double bboxWidth, bboxHeight;
};
```

## 训练到部署流程

1. 通过 DataCollectDialog 采集缺陷图像并标注
2. 使用 Python 训练 YOLO/分类模型
3. 导出 ONNX 模型
4. 将 `.onnx` 文件放入 `models/` 目录
5. AiInferencer 自动加载并通过 DNN 推理

## DefectInspectStep 集成

`DefectInspectStep` 在工作流中调用 AiInferencer，失败策略为 `ContinueWorkflow`（检测失败不中断生产）。`isEnabled()` 检查图像文件是否实际存在，避免无效推理。
