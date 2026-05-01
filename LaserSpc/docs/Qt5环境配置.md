# LaserSpc Qt5 环境配置说明

## 当前服务器环境

当前开发机环境如下：

1. 系统：`Ubuntu 24.04.3 LTS`
2. 编译器：`g++ 13.3.0`
3. CMake：`3.28.3`
4. Qt：`5.15.13`

## 已安装的软件包

为支持 `LaserSpc` 当前的 `Qt Widgets + QtSql + QtCharts + CMake` 开发，已安装以下关键包：

```bash
apt-get install -y \
  qtbase5-dev \
  qt5-qmake \
  qtbase5-dev-tools \
  qttools5-dev-tools \
  libqt5charts5-dev \
  libqt5sql5-sqlite \
  xvfb
```

## Qt5 CMake 配置路径

当前 Qt5 CMake 配置文件位于：

```bash
/usr/lib/x86_64-linux-gnu/cmake/Qt5/Qt5Config.cmake
```

在当前服务器环境下，`cmake` 已可直接找到 Qt5，一般不需要额外设置 `Qt5_DIR` 或 `CMAKE_PREFIX_PATH`。

## 项目配置与编译

在 `LaserSpc` 目录下执行：

```bash
cmake -S /root/qt-program/LaserSpc -B /root/qt-program/LaserSpc/build
cmake --build /root/qt-program/LaserSpc/build -j4
```

## 无界面服务器运行方式

由于当前机器是非 UI 的 Ubuntu Server，运行 Qt Widgets 程序时需要使用离屏平台：

```bash
QT_QPA_PLATFORM=offscreen /root/qt-program/LaserSpc/build/LaserSpc
```

如果后续需要在没有图形桌面的情况下跑界面相关测试，也可以使用：

```bash
xvfb-run -a /root/qt-program/LaserSpc/build/LaserSpc
```

## 当前验证结果

已完成以下验证：

1. `qmake --version` 可正常输出 `Qt 5.15.13`
2. `cmake` 可正常找到 `Qt5Config.cmake`
3. `LaserSpc` 可成功完成编译
4. `QT_QPA_PLATFORM=offscreen` 下可启动程序

## 说明

当前离屏运行日志中出现：

```text
This plugin does not support propagateSizeHints()
```

这属于离屏平台插件提示，不影响当前程序启动与开发验证。
