# LaserSpc MySQL 环境配置与仓储接入

## 1. 当前环境状态

当前服务器已经完成以下配置：

1. 安装 `MySQL Server 5.7+`（推荐与生产一致，项目 SQL 以 MySQL 5 兼容基线维护）
2. 安装 `MySQL Client`
3. 安装 Qt MySQL 驱动 `libqt5sql5-mysql`
4. 启动本机 `mysql` 服务
5. 初始化 `laser_spc` 数据库、表结构、测试账号和样例数据

## 2. 已安装的软件包

```bash
apt-get install -y mysql-server mysql-client libqt5sql5-mysql
```

## 3. 数据库信息

当前本地数据库配置如下：

```text
host=127.0.0.1
port=3306
database=laser_spc
user=laserspc
password=LaserSpc#2026
```

## 4. SQL 脚本位置

初始化脚本都在：

```text
/root/qt-program/LaserSpc/sql/mysql
```

包含：

1. `migrations/`
2. `seed_data.sql`
3. `seed_point_records.sql`

## 5. 数据库重建

```bash
D:\Program\spc\LaserSpc\build-msvc-qt5\Debug\LaserSpcDbInit.exe
```

说明：

1. 当前不再建议手工串行执行旧版初始化 SQL。
2. 建库、建用户、授权、迁移和种子导入统一由 `LaserSpcDbInit.exe` 处理。
3. `sql/mysql/migrations` 负责表结构版本升级。
4. `seed_data.sql` 与 `seed_point_records.sql` 仅保留给初始化工具和回归恢复流程使用。

## 6. 当前表与样例数据

当前已创建的核心表：

1. `board_records`
2. `point_records`

当前样例数据数量：

1. `board_records`: `10`
2. `point_records`: `10`

## 7. 代码接入说明

当前代码已新增以下组件：

1. `DatabaseConnection`
   - 负责创建 `QMYSQL` 连接
2. `MySqlSpcRepository`
   - 实现总览、不良统计、单板记录、点位记录查询
3. `AppConfigService`
   - 提供 MySQL 默认配置和环境变量覆盖能力
4. `main.cpp`
   - 启动时优先连接 MySQL，失败才回退 `MockSpcRepository`

## 8. 环境变量覆盖

如需修改连接信息，可在启动前设置：

```bash
export LASERSPC_USE_MYSQL=1
export LASERSPC_DB_HOST=127.0.0.1
export LASERSPC_DB_PORT=3306
export LASERSPC_DB_NAME=laser_spc
export LASERSPC_DB_USER=laserspc
export LASERSPC_DB_PASSWORD='LaserSpc#2026'
```

关闭 MySQL 仓储并回退假数据：

```bash
export LASERSPC_USE_MYSQL=0
```

## 9. 验证命令

编译：

```bash
cmake -S /root/qt-program/LaserSpc -B /root/qt-program/LaserSpc/build
cmake --build /root/qt-program/LaserSpc/build -j4
```

无界面验证运行：

```bash
QT_QPA_PLATFORM=offscreen /root/qt-program/LaserSpc/build/LaserSpc
```

如果日志中出现：

```text
[LaserSpc] Using data source: MySQL Repository
```

说明当前程序已经走真实 MySQL 仓储。

## 10. 无界面服务器的主机配置建议

在当前 Ubuntu Server 环境中，建议优先使用 `127.0.0.1` 而不是 `localhost`：

```text
host=127.0.0.1
```

原因是 `localhost` 在 MySQL 客户端中可能优先走 Unix Socket，而服务器版环境、沙箱或服务重启后，socket 路径不一致时会表现为“服务在跑，但应用连不上”。使用 `127.0.0.1` 可以显式走 TCP，更适合当前项目的开发和测试环境。
