# Qt5 Windows 发布与自检清单

适用环境：

- Windows 10/11 x64
- Qt 5.15.2 `msvc2019_64`
- MariaDB Connector/C x64
- LaserSpc 固定使用 Qt5 运行

## 1. 生成发布目录

先完成 Qt5 构建，至少要有以下产物：

- `LaserSpc.exe`
- `LaserSpcDbInit.exe`
- `LaserSpcPerfTool.exe`

执行发布脚本：

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\tools\Prepare-Qt5Deployment.ps1 -BuildDir .\build-msvc-qt5 -Config Release
```

如果当前只有调试版本，也可以先出一份调试部署目录：

```powershell
.\tools\Prepare-Qt5Deployment.ps1 -BuildDir .\build-msvc-qt5 -Config Debug
```

默认输出目录：

```text
build-msvc-qt5\deploy-qt5\Release
```

## 2. 发布目录最少应包含

- `LaserSpc.exe`
- `Qt5Core.dll`
- `Qt5Gui.dll`
- `Qt5Widgets.dll`
- `Qt5Sql.dll`
- `platforms\qwindows.dll`
- `sqldrivers\qsqlmysql.dll` 或 `sqldrivers\qsqlmysqld.dll`
- `libmariadb.dll`
- `config\laserspc.ini`

脚本会自动输出 `deployment-check.txt`，用于留档和新机器自检。

## 3. 首次运行前检查

- `config\laserspc.ini` 中 MySQL 地址、端口、用户名、密码是否正确
- `connect_options` 是否包含当前机器的 MariaDB plugin 目录
- 目标机器是否已安装 VC++ 运行时
- `qsqlmysql.dll` 是否为当前 Qt5.15.2 构建出来的版本，而不是其他 Qt 版本混入

当前推荐的 `connect_options`：

```ini
MYSQL_OPT_SSL_ENFORCE=0;MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0;MYSQL_PLUGIN_DIR=C:/Program Files/MariaDB/MariaDB Connector C 64-bit/lib/plugin
```

## 4. 上线前自检顺序

1. 运行 `LaserSpcDbInit.exe`，确认数据库初始化成功。
2. 启动 `LaserSpc.exe`，确认首页可以正常加载筛选项。
3. 切换到汇总页、不良统计页、单板记录页、点位记录页，确认无空白页和驱动报错。
4. 修改基础设置并保存，确认数据源切换与配置回写正常。
5. 运行一轮 `LaserSpcPerfTool.exe --mode bench`，确认关键 SQL 耗时没有异常飙升。

## 5. 新机器常见问题

- 启动时报 `Driver not loaded`：
  检查 `sqldrivers\qsqlmysql.dll`、`libmariadb.dll`、`MYSQL_PLUGIN_DIR` 三项是否同时到位。
- 启动时报 SSL 或 plugin 相关错误：
  优先检查 `connect_options` 是否完整。
- 程序能启动但 MySQL 页面无数据：
  先看 `config\laserspc.ini`，再检查数据库账号是否对目标库有权限。
- Release 目录缺文件：
  重新执行 `Prepare-Qt5Deployment.ps1`，不要手工零散拷贝。
