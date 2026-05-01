# LaserSpc Windows 开发与 Qt5/Qt6 兼容说明

## 1. 当前项目实际状态

结合 `方案输出`、`docs/开发规划.md` 和当前代码，`LaserSpc` 已经不是“完整镭雕上位机”，而是一个 `SPC 查询看板 MVP`。当前已经落地的内容主要是：

1. `MainWindow + MainShellMediator` 主壳层和页面切换
2. `FilterPanel` 公共筛选栏
3. 四个业务页
   - 数据总览
   - 不良统计
   - 单板记录
   - 点位记录
4. `AppServiceFacade` 统一查询入口
5. `ISpcQueryRepository` 仓储抽象
6. `MockSpcRepository` 假数据链路
7. `MySqlSpcRepository` 真实 MySQL 查询链路
8. `ExportService` CSV / 截图导出
9. `SettingsDialog + AppConfigService` 基础配置持久化
10. 一组覆盖 UI 联动、仓储、导出和回退策略的自动化测试

当前还没有落地的内容：

1. 设备协议接入
2. 实时采集和入库链路
3. MES 接口
4. 完整权限系统
5. 自动清理归档
6. 多语言
7. 复杂报表

## 2. Windows 继续开发的核心结论

当前仓库可以在 Windows 上继续开发，但要满足下面几个前提：

1. 使用统一的 `CMake` 工程，不再回到 `.sln + vcxproj` 双维护模式
2. Windows 上必须安装可用的 Qt 桌面套件
3. 真实 MySQL 调试需要 `Qt SQL MySQL` 驱动插件
4. 编译器要和 Qt 套件一致
   - Qt `msvc2022_64` 对应 Visual Studio 2022 MSVC
   - 不要拿 MinGW 去编译 `msvc2022_64` 套件

## 3. 当前 Windows 环境核查结果

当前机器已经确认：

1. 已安装 `CMake 4.1.1`
2. 已安装 `Qt 6.9.2 msvc2022_64`
3. 已存在 Visual Studio 2022 的 `vcvars64.bat`
   - `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat`

当前机器还没有确认可直接使用的内容：

1. 命令行默认环境里没有 `cl`
   - 需要先进入 VS Developer 环境，或者让 IDE 自动注入编译环境
2. 当前 `C:\Qt` 下未发现 Qt5 套件
3. 当前 Qt6 插件目录下未发现 `qsqlmysql.dll`
   - 这意味着程序在 Windows 上大概率会回退到 `Mock Repository`
   - 如果你要接真实 MySQL，必须补上 Qt 的 MySQL 驱动

## 4. 已做的兼容调整

为了兼容 Qt5 和 Qt6，当前工程已经做了以下改动：

1. `CMakeLists.txt`
   - 从写死 `Qt5::*` 改为 `Qt${QT_VERSION_MAJOR}::*`
   - `find_package` 改成优先找 Qt6，找不到再回退 Qt5
2. `main.cpp`
   - `Qt::AA_EnableHighDpiScaling` 只在 Qt5 下启用
3. `AppConfigService.cpp`
   - `QSettings::setIniCodec("UTF-8")` 仅在 Qt5 下调用
4. `ExportService.cpp`
   - `QTextStream::setCodec("UTF-8")` 改成 Qt5/Qt6 双分支

这些改动的目标是：

1. Windows + Qt6 可以编译
2. Ubuntu / Qt5 旧环境不被破坏
3. 代码仍然维持一套，不拆分两套分支

## 5. 推荐的开发矩阵

建议把环境分成两条线：

1. 主开发线：`Windows + Qt6 + MSVC 2022`
   - 用于当前机器日常开发、界面调试、打包
2. 兼容验证线：`Ubuntu + Qt5.15.x`
   - 用于持续确认旧环境不回归

这样做比“强行只在一个平台做全部验证”更稳，因为你的现状是：

1. Windows 机器现成的是 Qt6
2. Ubuntu 机器现成的是 Qt5
3. 项目本身又要求两边兼容

## 6. Windows 首次配置建议

### 6.1 先补齐 Qt5

如果你要在 Windows 上同时本地验证 Qt5 和 Qt6，建议在 Qt Maintenance Tool 里额外安装：

1. `Qt 5.15.x`
2. `msvc2022_64` 或与你 Qt6 相同编译器链的桌面套件
3. `Qt Charts`
4. `Qt SQL` 相关组件

重点是编译器必须统一，否则你会得到“两套 Qt 都装了，但只有一套能编译”的伪兼容状态。

### 6.2 补齐 MySQL 驱动

如果你要在 Windows 上连接真实 MySQL，需要确认 Qt 插件目录存在：

```text
<QtDir>\plugins\sqldrivers\qsqlmysql.dll
```

如果没有，常见处理方式有两种：

1. 通过 Qt 安装器安装对应的 MySQL 驱动组件
2. 用当前 Qt 源码和本机 MySQL Connector/C 重新编译 `sqldrivers`

如果这一步没完成，程序依旧能启动，但会自动回退到 `Mock Repository`。

## 7. Windows 下推荐构建方式

### 7.1 Qt6 构建

```powershell
cmd /c "\"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat\" && cmake -S D:\Program\spc\LaserSpc -B D:\Program\spc\LaserSpc\build-msvc-qt6 -G \"Visual Studio 17 2022\" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.9.2\msvc2022_64 && cmake --build D:\Program\spc\LaserSpc\build-msvc-qt6 --config Debug"
```

### 7.2 Qt5 构建

```powershell
cmd /c "\"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat\" && cmake -S D:\Program\spc\LaserSpc -B D:\Program\spc\LaserSpc\build-msvc-qt5 -G \"Visual Studio 17 2022\" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\5.15.x\msvc2022_64 && cmake --build D:\Program\spc\LaserSpc\build-msvc-qt5 --config Debug"
```

如果你使用 Qt Creator，可以直接配置两套 Kit：

1. `MSVC 2022 + Qt 6.9.2`
2. `MSVC 2022 + Qt 5.15.x`

然后切换同一个 `CMakeLists.txt` 即可。

## 8. Windows 运行与部署建议

开发态运行时，至少要保证：

1. Qt 的运行时 DLL 可被找到
2. `platforms/qwindows.dll` 在可搜索路径中
3. 如果使用数据库，`sqldrivers` 目录完整

推荐在构建完成后执行：

```powershell
C:\Qt\6.9.2\msvc2022_64\bin\windeployqt.exe D:\Program\spc\LaserSpc\build-msvc-qt6\Debug\LaserSpc.exe
```

Qt5 则换成对应 Qt5 套件里的 `windeployqt.exe`。

## 9. 继续开发时的兼容约束

后续新代码要遵守下面几条，否则很快又会把双版本兼容打破：

1. 不要在 `CMakeLists.txt` 里重新写死 `Qt5::` 或 `Qt6::`
2. 遇到 Qt API 差异，优先用 `QT_VERSION_CHECK` 做小范围兼容
3. 不要引入 Qt6 专属模块，除非同时提供 Qt5 降级方案
4. UI 层继续只依赖 `AppServiceFacade`
5. 数据访问继续收敛在 Repository 层
6. Windows 特有路径、编码、换行问题不要写死在业务逻辑里

## 10. 最推荐的推进顺序

建议按这个顺序推进，而不是一次把全部环境问题混在一起：

1. 先在 Windows Qt6 下跑通 `Mock Repository`
2. 再补上 Windows 的 `qsqlmysql.dll`，跑通真实 MySQL
3. 再安装 Windows Qt5，做第二套构建验证
4. 最后把这两套构建加进持续集成或至少形成固定自测清单

## 11. 结论

从当前仓库状态看，`LaserSpc` 已经具备在 Windows 上继续开发的基础，且更适合先以 `Windows + Qt6 + MSVC2022` 为主环境推进。

但要做到你说的“完美运行并继续开发”，还差两个现实条件：

1. Windows 上补齐 Qt5 套件
2. Windows 上补齐 MySQL Qt 驱动

如果这两个条件不补，项目仍然可以开发和运行，但只能做到：

1. `Qt6` 本地开发
2. `Qt5` 依赖 Ubuntu 验证
3. Windows 上真实数据库功能可能回退到假数据
