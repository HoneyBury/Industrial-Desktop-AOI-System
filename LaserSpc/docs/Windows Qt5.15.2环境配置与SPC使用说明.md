# LaserSpc Windows Qt5.15.2 环境配置与 SPC 使用说明

本文档用于新机器首次部署 `LaserSpc` 时的标准操作。

适用前提：

1. 上位机固定使用 `Windows`
2. Qt 固定使用 `Qt 5.15.2`
3. 项目目录固定为 `D:\Program\spc\LaserSpc`
4. 数据源优先使用真实 MySQL，而不是 Mock Repository

本文档内容基于当前项目已经实际验证通过的环境，不是理论流程。

## 1. 已验证通过的基线环境

当前已经跑通的组合如下：

1. 操作系统：Windows
2. Qt：`C:\Qt\5.15.2\msvc2019_64`
3. Qt Sources：`C:\Qt\5.15.2\Src`
4. 编译器：Visual Studio 2019 Build Tools x64
5. MariaDB Connector/C x64：`C:\Program Files\MariaDB\MariaDB Connector C 64-bit`
6. 项目：`D:\Program\spc\LaserSpc`
7. 本地 MySQL 访问地址：`127.0.0.1:9527`

当前已经验证通过：

1. Qt5 项目可编译
2. Qt5 `QMYSQL` 驱动可编译并安装
3. `laser_spc` 数据库可初始化
4. 真实 MySQL 集成回归测试通过
5. `LaserSpc.exe` 可在 Qt5 环境下实际启动

## 2. 新机器需要安装的内容

新机器至少需要准备以下组件：

1. Qt 5.15.2 Desktop MSVC 64-bit
2. Qt 5.15.2 Sources
3. Visual Studio 2019 Build Tools
4. CMake
5. MariaDB Connector/C x64
6. 本地 MySQL 或可访问的 MySQL 服务

建议安装路径保持一致：

1. Qt 安装在 `C:\Qt`
2. 项目放在 `D:\Program\spc`
3. MariaDB Connector/C 使用默认安装路径

## 3. 项目中已经提供的辅助文件

当前仓库里已经提供以下部署辅助文件：

1. Qt5 `QMYSQL` 编译脚本：[build-qmysql-qt5.cmd](/D:/Program/spc/LaserSpc/tools/build-qmysql-qt5.cmd)
2. Qt5 `QMYSQL` 外部补丁脚本：[Setup-Qt5MySqlDriver.ps1](/D:/Program/spc/LaserSpc/tools/Setup-Qt5MySqlDriver.ps1)
3. 数据库初始化工具源码：[LaserSpcDbInit.cpp](/D:/Program/spc/LaserSpc/tools/LaserSpcDbInit.cpp)
4. Windows Qt5 示例配置：[laserspc.windows.qt5.example.ini](/D:/Program/spc/LaserSpc/config/laserspc.windows.qt5.example.ini)
5. MySQL 迁移脚本目录：[migrations](/D:/Program/spc/LaserSpc/sql/mysql/migrations)
6. MySQL 种子数据脚本：[seed_data.sql](/D:/Program/spc/LaserSpc/sql/mysql/seed_data.sql)
7. MySQL 点位补种脚本：[seed_point_records.sql](/D:/Program/spc/LaserSpc/sql/mysql/seed_point_records.sql)

## 4. 首次环境配置步骤

### 4.1 编译 Qt5 项目

在 PowerShell 中执行：

```powershell
cmake -S D:\Program\spc\LaserSpc -B D:\Program\spc\LaserSpc\build-msvc-qt5 -G "Visual Studio 16 2019" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64
cmake --build D:\Program\spc\LaserSpc\build-msvc-qt5 --config Debug --target LaserSpc LaserSpcTests LaserSpcDbInit
```

构建产物位置：

1. 主程序：[LaserSpc.exe](/D:/Program/spc/LaserSpc/build-msvc-qt5/Debug/LaserSpc.exe)
2. 测试程序：[LaserSpcTests.exe](/D:/Program/spc/LaserSpc/build-msvc-qt5/Debug/LaserSpcTests.exe)
3. 数据库初始化工具：[LaserSpcDbInit.exe](/D:/Program/spc/LaserSpc/build-msvc-qt5/Debug/LaserSpcDbInit.exe)

### 4.2 为 Qt5 安装 QMYSQL 驱动

首次新机器部署时，必须安装 Qt5 的 `QMYSQL` 驱动，否则程序只能回退到 Mock 数据。

先执行补丁脚本：

```powershell
powershell -ExecutionPolicy Bypass -File D:\Program\spc\LaserSpc\tools\Setup-Qt5MySqlDriver.ps1
```

如果希望补丁后直接编译并安装驱动：

```powershell
powershell -ExecutionPolicy Bypass -File D:\Program\spc\LaserSpc\tools\Setup-Qt5MySqlDriver.ps1 -BuildAndInstall
```

也可以分两步执行：

```powershell
powershell -ExecutionPolicy Bypass -File D:\Program\spc\LaserSpc\tools\Setup-Qt5MySqlDriver.ps1
D:\Program\spc\LaserSpc\tools\build-qmysql-qt5.cmd install
```

安装完成后，确认以下文件存在：

1. [`qsqlmysql.dll`](/C:/Qt/5.15.2/msvc2019_64/plugins/sqldrivers)
2. [`qsqlmysqld.dll`](/C:/Qt/5.15.2/msvc2019_64/plugins/sqldrivers)

说明：

1. 这里之所以需要补丁，不是项目代码问题，而是 Qt5 原始 `QMYSQL` 对当前 Windows + MariaDB Connector/C + MySQL 认证插件组合支持不完整
2. 补丁脚本会处理两个外部点：
   `qsql_mysql.cpp`
   `qmodule.pri`

### 4.3 准备运行时 PATH

运行前至少要让以下目录进入 `PATH`：

```text
C:\Qt\5.15.2\msvc2019_64\bin
C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib
```

如果是手工临时启动，可以在 PowerShell 中执行：

```powershell
$env:PATH="C:\Qt\5.15.2\msvc2019_64\bin;C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib;$env:PATH"
```

## 5. 数据库初始化步骤

### 5.1 当前已验证的数据库访问参数

当前本机已经验证通过的参数为：

```text
host=127.0.0.1
port=9527
database=laser_spc
user=laserspc
password=LaserSpc#2026
```

说明：

1. `root / zjh123456` 只建议用于初始化数据库
2. 应用实际连接建议使用 `laserspc`
3. 当前项目会在初始化流程中自动创建 `laserspc@localhost` 和 `laserspc@%`

### 5.2 使用项目工具初始化数据库

推荐使用项目自带工具完成数据库重建，而不是手工拆分执行 SQL：

```powershell
$env:LASERSPC_DB_HOST='127.0.0.1'
$env:LASERSPC_DB_PORT='3306'
$env:LASERSPC_ADMIN_DB='mysql'
$env:LASERSPC_ADMIN_DB_USER='root'
$env:LASERSPC_ADMIN_DB_PASSWORD='zjh123456'
$env:LASERSPC_DB_NAME='laser_spc'
$env:LASERSPC_DB_USER='laserspc'
$env:LASERSPC_DB_PASSWORD='LaserSpc#2026'
$env:LASERSPC_DB_SEED='1'
$env:LASERSPC_DB_CONNECT_OPTIONS='MYSQL_OPT_SSL_ENFORCE=0;MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0'
$env:PATH="C:\Qt\5.15.2\msvc2019_64\bin;C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib;$env:PATH"

D:\Program\spc\LaserSpc\build-msvc-qt5\Debug\LaserSpcDbInit.exe
```

成功输出示例：

```text
MySQL bootstrap completed: 127.0.0.1:3306 -> laser_spc
```

这个工具会按顺序处理：

1. [`LaserSpcDbInit.exe`](/D:/Program/spc/LaserSpc/build-msvc-qt5/Debug/LaserSpcDbInit.exe)
2. [`migrations`](/D:/Program/spc/LaserSpc/sql/mysql/migrations)
3. [`seed_data.sql`](/D:/Program/spc/LaserSpc/sql/mysql/seed_data.sql)
4. [`seed_point_records.sql`](/D:/Program/spc/LaserSpc/sql/mysql/seed_point_records.sql)

## 6. SPC 运行配置说明

### 6.1 配置文件位置

应用默认从程序目录下读取：

```text
config/laserspc.ini
```

Qt5 Debug 构建目录中建议使用：

[`laserspc.ini`](/D:/Program/spc/LaserSpc/build-msvc-qt5/Debug/config/laserspc.ini)

也可以直接复制示例文件：

[`laserspc.windows.qt5.example.ini`](/D:/Program/spc/LaserSpc/config/laserspc.windows.qt5.example.ini)

### 6.2 推荐配置内容

当前 Windows Qt5 推荐配置如下：

```ini
[general]
use_mysql=true
default_query_days=7
auto_refresh_enabled=false
auto_refresh_interval_seconds=60

[mysql]
host=127.0.0.1
port=9527
database_name=laser_spc
user_name=laserspc
password=LaserSpc#2026
connect_options=MYSQL_OPT_SSL_ENFORCE=0;MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0;MYSQL_PLUGIN_DIR=C:/Program Files/MariaDB/MariaDB Connector C 64-bit/lib/plugin
```

字段说明：

1. `use_mysql=true`
   启用真实 MySQL 仓储
2. `default_query_days`
   默认查询最近几天
3. `auto_refresh_enabled`
   是否自动刷新
4. `auto_refresh_interval_seconds`
   自动刷新间隔秒数
5. `connect_options`
   当前 Windows Qt5 环境下必须保留

### 6.3 connect_options 为什么不能删

当前 Windows Qt5 实际运行中，这个字段用于解决三个问题：

1. `MYSQL_OPT_SSL_ENFORCE=0`
   关闭强制 SSL，避免 `SEC_E_NO_CREDENTIALS`
2. `MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0`
   避免证书校验问题
3. `MYSQL_PLUGIN_DIR=...`
   让客户端能找到认证插件，例如 `caching_sha2_password.dll`

如果删掉这个字段，常见后果是：

1. MySQL 能 ping 通，但 Qt 连不上
2. 报 TLS/SSL 错误
3. 报认证插件找不到

## 7. SPC 使用说明

### 7.1 启动方式

在已设置好 `PATH` 的 PowerShell 里执行：

```powershell
$env:LASERSPC_CONFIG_FILE='D:\Program\spc\LaserSpc\build-msvc-qt5\Debug\config\laserspc.ini'
$env:PATH="C:\Qt\5.15.2\msvc2019_64\bin;C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib;$env:PATH"

D:\Program\spc\LaserSpc\build-msvc-qt5\Debug\LaserSpc.exe
```

### 7.2 程序中的基础设置

程序右上角有“基础设置”入口，可以修改：

1. 是否优先使用 MySQL
2. 主机
3. 端口
4. 数据库名
5. 用户名
6. 密码
7. 默认查询天数
8. 自动刷新开关和间隔

当前代码已经处理过一个关键问题：

1. 从设置对话框保存配置时，会保留已有 `connect_options`
2. 不会因为 UI 保存一次就把 MySQL 连接选项冲掉

### 7.3 四个核心页面

当前 `LaserSpc` 实际落地的是一个 `SPC 查询看板 MVP`，包含四个页面：

1. 数据总览
2. 不良统计
3. 单板记录
4. 点位记录

主要使用方式：

1. 先在公共筛选栏选择时间范围
2. 按产线、程序名、设备名、结果做过滤
3. 在总览页查看良率和汇总
4. 在不良统计页查看不良点和等级分布
5. 在单板记录页查看板级记录
6. 在点位记录页查看点位级明细

## 8. 新机器部署后的验证步骤

### 8.1 最小验证

先验证数据库环境：

```powershell
$env:LASERSPC_REQUIRE_MYSQL='1'
$env:LASERSPC_DB_HOST='127.0.0.1'
$env:LASERSPC_DB_PORT='9527'
$env:LASERSPC_DB_NAME='laser_spc'
$env:LASERSPC_DB_USER='laserspc'
$env:LASERSPC_DB_PASSWORD='LaserSpc#2026'
$env:LASERSPC_DB_CONNECT_OPTIONS='MYSQL_OPT_SSL_ENFORCE=0;MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0;MYSQL_PLUGIN_DIR=C:/Program Files/MariaDB/MariaDB Connector C 64-bit/lib/plugin'
$env:QT_QPA_PLATFORM='offscreen'
$env:PATH="C:\Qt\5.15.2\msvc2019_64\bin;C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib;$env:PATH"

D:\Program\spc\LaserSpc\build-msvc-qt5\Debug\LaserSpcTests.exe mysqlEnvironmentSmokeTest
```

### 8.2 完整 MySQL 回归

```powershell
$env:LASERSPC_REQUIRE_MYSQL='1'
$env:LASERSPC_DB_HOST='127.0.0.1'
$env:LASERSPC_DB_PORT='9527'
$env:LASERSPC_DB_NAME='laser_spc'
$env:LASERSPC_DB_USER='laserspc'
$env:LASERSPC_DB_PASSWORD='LaserSpc#2026'
$env:LASERSPC_DB_CONNECT_OPTIONS='MYSQL_OPT_SSL_ENFORCE=0;MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0;MYSQL_PLUGIN_DIR=C:/Program Files/MariaDB/MariaDB Connector C 64-bit/lib/plugin'
$env:QT_QPA_PLATFORM='offscreen'
$env:PATH="C:\Qt\5.15.2\msvc2019_64\bin;C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib;$env:PATH"

D:\Program\spc\LaserSpc\build-msvc-qt5\Debug\LaserSpcTests.exe `
  mysqlRepositoryProvidesExpectedFilterOptions `
  mysqlSummaryMetricsMatchSeedData `
  mysqlBadStatisticsMatchSeedData `
  mysqlPointRepositorySupportsFilteringAndSorting `
  summaryRepositorySupportsPagination `
  boardRepositorySupportsSorting `
  summaryPageLoadsRealMySqlData `
  pointRecordPageLoadsRealMySqlData `
  mainWindowLoadsRealMySqlDataAndRefreshesCurrentPage
```

当前这组测试已经在本机通过。

## 9. 常见问题

### 9.1 启动后回退到 Mock Repository

优先检查：

1. `qsqlmysql.dll` 是否已安装
2. `libmariadb.dll` 是否能被找到
3. `connect_options` 是否还在
4. 数据库地址和端口是否正确

### 9.2 提示 TLS/SSL error

说明 `connect_options` 没生效，确认：

1. `MYSQL_OPT_SSL_ENFORCE=0`
2. `MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0`

### 9.3 提示 caching_sha2_password 插件找不到

确认：

1. `MYSQL_PLUGIN_DIR=C:/Program Files/MariaDB/MariaDB Connector C 64-bit/lib/plugin`
2. 该目录下存在 `caching_sha2_password.dll`

### 9.4 提示 Unknown database 'laser_spc'

说明数据库还没有初始化，重新执行：

[`LaserSpcDbInit.exe`](/D:/Program/spc/LaserSpc/build-msvc-qt5/Debug/LaserSpcDbInit.exe)

### 9.5 测试里出现字体目录警告

当前 Qt5 测试会有字体目录警告：

```text
QFontDatabase: Cannot find font directory C:/Qt/5.15.2/msvc2019_64/lib/fonts
```

这不会阻塞当前 SPC 功能和 MySQL 回归，可以先忽略。

## 10. 推荐的交付清单

以后每台新机器部署完成后，建议至少确认以下结果：

1. Qt5 项目可编译
2. `QMYSQL` 驱动已安装
3. `LaserSpcDbInit.exe` 可成功初始化数据库
4. `mysqlEnvironmentSmokeTest` 通过
5. MySQL integration tests 通过
6. `LaserSpc.exe` 可正常启动
7. `config/laserspc.ini` 已配置好真实数据库参数

满足以上 7 项，基本就可以认为这台 Windows 上位机已经具备继续开发和现场使用的基础条件。
