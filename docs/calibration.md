# 标定模块设计

标定模块已从占位桩升级为真实 OpenCV 实现，共 5 个独立标定器。

## 标定器概览

| 标定器 | 输入 | 算法 | 输出 |
|--------|------|------|------|
| `CameraIntrinsicCalibrator` | 棋盘格图像目录 | `cv::findChessboardCorners` + `cv::cornerSubPix` + `cv::calibrateCamera` | fx, fy, cx, cy, 畸变系数 |
| `PixelScaleCalibrator` | 像素距离 + 物理距离 | 除法：physical/pixel | pixelToMillimeterX/Y |
| `OriginCalibrator` | 图像参考点 + 机械参考位姿 | 像素偏移→mm→机械位姿修正 | 原点修正位姿 |
| `LaserOffsetCalibrator` | 十字中心检测 | Camera-to-Laser 固定偏移 | cameraToLaserDx/Dy |
| `MarkReferenceCalibrator` | Mark 点定义 + 像素比例 | Mark→MarkReferenceRecord 转换 | MarkReferenceRecord 列表 |

## CameraIntrinsicCalibrator 详情

- 棋盘格规格：9×6 内角点，25mm 方格
- 支持格式：`.png`、`.jpg`、`.jpeg`、`.bmp`
- 亚像素精化：`cv::cornerSubPix` (11×11 窗口)
- 最少图像数：3 张（不足时回退到引导值 fx=1000, fy=1000）
- 输出：fx, fy, cx, cy, 畸变系数向量, RMS 重投影误差
- 支持 save/load 持久化

## 标定数据流

```
棋盘格图像目录 → CameraIntrinsicCalibrator → CameraIntrinsicCalibration
已知距离测量 → PixelScaleCalibrator → PixelScaleCalibration
图像参考点 + 机械位姿 → OriginCalibrator → OriginCalibration
十字中心图像 → LaserOffsetCalibrator → LaserOffsetCalibration
Mark 点定义 → MarkReferenceCalibrator → MarkReferenceRecord[]
```

所有标定数据存储在 `ProgramModel` 中，通过 `ProgramManager` JSON 序列化持久化。
