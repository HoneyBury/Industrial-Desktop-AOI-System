# OpenCV 接入约定

当前项目已经支持优先从 `third_party/opencv` 自动查找 OpenCV。

在 macOS 上，如果仓库内没有放置 OpenCV，项目也会额外尝试读取 Homebrew 默认路径：

- `/opt/homebrew/opt/opencv`
- `/usr/local/opt/opencv`

## 推荐目录结构

你可以把 OpenCV 放成下面任意一种形式：

1. 直接把安装结果放在这里

```text
third_party/opencv/
  lib/cmake/opencv4/OpenCVConfig.cmake
```

2. 把源码构建后的 install 目录放在这里

```text
third_party/opencv/install/
  lib/cmake/opencv4/OpenCVConfig.cmake
```

3. 把预编译包解压到这里

```text
third_party/opencv/<platform>/
  lib/cmake/opencv4/OpenCVConfig.cmake
```

## macOS 推荐做法

如果你是在 MacBook 上开发，并且希望当前项目真正连接笔记本前置摄像头，建议使用带 `videoio` 的 OpenCV 构建，并确保启用了系统默认的 `AVFoundation` 视频后端。

本项目中的 `UsbCamera` 已经优先在 macOS 上使用：

- `cv::CAP_AVFOUNDATION`
- 打不开时回退到 `cv::CAP_ANY`

## 配置后如何验证

```bash
cmake --preset default
cmake --build --preset default --parallel
```

如果配置成功，`cmake` 输出里会看到类似：

```text
OpenCV enabled from: ...
```
