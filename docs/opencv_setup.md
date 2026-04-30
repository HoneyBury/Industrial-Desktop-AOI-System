# OpenCV 本地接入说明

## 目标

让当前项目不依赖全局环境变量，优先从仓库内的 `third_party/opencv` 读取 OpenCV，并使 `UsbCamera` 真正走 OpenCV 的实时采图路径。

## 当前工程行为

- 若存在 `third_party/opencv/.../OpenCVConfig.cmake`，`CMake` 会优先解析项目内 OpenCV
- 若当前机器是 macOS，项目也会额外尝试 Homebrew 默认路径：
  `/opt/homebrew/opt/opencv`、`/usr/local/opt/opencv`
- 若用户手工传入 `OpenCV_DIR`，项目尊重用户显式配置
- 若两者都不存在，则项目继续退化为无 OpenCV 的 stub 模式

## 推荐目录

```text
third_party/
  opencv/
    install/
      lib/cmake/opencv4/OpenCVConfig.cmake
```

## macOS 构建建议

如果你准备自己编译 OpenCV，再放进 `third_party/opencv/install`，建议至少包含这些模块：

- `core`
- `imgproc`
- `videoio`
- `objdetect`

对于笔记本前置摄像头，macOS 下最关键的是 `videoio` 可用，并带上 `AVFoundation`。

## 运行效果

启用 OpenCV 后，`src/camera/UsbCamera.cpp` 会：

1. 优先使用 `cv::CAP_AVFOUNDATION` 打开设备
2. 回退到 `cv::CAP_ANY`
3. 按 UI 当前分辨率预设设置采图尺寸

## 常见问题

### 1. 构建没报错，但仍是模拟采图

先看 `cmake` 配置输出里是否出现：

```text
OpenCV enabled from: ...
```

如果没有，说明 `OpenCVConfig.cmake` 还没被项目找到。

### 2. 摄像头打不开

- 先确认 macOS 已给终端或 IDE 摄像头权限
- 先尝试设备索引 `0`
- 若有外接摄像头，再尝试 `1`、`2`

### 3. 预设分辨率未生效

不同摄像头和后端对 `CAP_PROP_FRAME_WIDTH/HEIGHT` 的支持程度不同，项目目前会尽力设置，但实际返回尺寸仍以摄像头驱动能力为准
