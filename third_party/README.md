# 第三方依赖目录

这个目录用于放置项目级第三方依赖，优先服务于“本仓库自带可配置依赖”的开发方式，避免每台机器都手工改系统路径。

当前已经约定：

- `third_party/opencv/`
  OpenCV 的本地安装目录、预编译包解压目录，或源码构建后的 install 目录

项目的 `CMakeLists.txt` 会优先从这里自动解析 `OpenCVConfig.cmake`。
