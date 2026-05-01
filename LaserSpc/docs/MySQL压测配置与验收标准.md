# MySQL压测配置与验收标准

本文用于统一 LaserSpc 在 Windows + Qt 5.15.2 环境下的 MySQL 压测方式、配置基线和验收口径。

## 1. 适用范围

- 上位机运行环境固定为 Windows x64
- Qt 运行时固定为 Qt 5.15.2 `msvc2019_64`
- MySQL 访问通过 Qt5 `QMYSQL` 驱动
- 压测工具固定使用 `LaserSpcPerfTool.exe`

## 2. 压测前置配置

推荐直接加载项目内的示例配置脚本：

```powershell
. .\config\laserspc.perf.qt5.example.ps1
```

示例配置文件：

- [laserspc.perf.qt5.example.ps1](/D:/Program/spc/LaserSpc/config/laserspc.perf.qt5.example.ps1)

关键环境变量说明：

- `LASERSPC_DB_HOST`
  MySQL 主机地址，当前基线为 `127.0.0.1`
- `LASERSPC_DB_PORT`
  MySQL 端口，当前基线为 `9527`
- `LASERSPC_DB_CONNECT_OPTIONS`
  Qt5 Windows 连接 MySQL 的关键参数，当前必须保留
- `LASERSPC_ADMIN_USER` / `LASERSPC_ADMIN_PASSWORD`
  用于创建压测库
- `LASERSPC_TARGET_USER` / `LASERSPC_TARGET_PASSWORD`
  用于连接压测库并执行造数、压测和 EXPLAIN
- `LASERSPC_PERF_DB_NAME`
  专用压测库名，推荐固定为 `laser_spc_perf`

## 3. 压测命令

### 3.1 基线压测

```powershell
.\build-msvc-qt5\Debug\LaserSpcPerfTool.exe --mode all --db-name laser_spc_perf --boards 5000 --points-per-board 4 --rounds 5 --top-n 10
```

该命令会自动完成：

1. 创建压测库
2. 检查表结构和补充索引
3. 生成压测数据
4. 执行关键查询 benchmark
5. 输出关键 SQL 的 EXPLAIN

### 3.2 仅跑 benchmark

```powershell
.\build-msvc-qt5\Debug\LaserSpcPerfTool.exe --mode bench --db-name laser_spc_perf --boards 5000 --points-per-board 4 --rounds 5 --top-n 10
```

### 3.3 仅看执行计划

```powershell
.\build-msvc-qt5\Debug\LaserSpcPerfTool.exe --mode explain --db-name laser_spc_perf --boards 5000 --points-per-board 4 --rounds 5 --top-n 10
```

## 4. 压测数据标准

建议统一分三档：

### 4.1 快速冒烟

- `boards=1000`
- `points-per-board=4`
- 总点位数约 `4000`
- 目的：确认驱动、权限、建库、建表、索引补齐、基本查询都正常

### 4.2 当前项目基线

- `boards=5000`
- `points-per-board=4`
- 总点位数约 `20000`
- `rounds=5`
- 目的：作为日常回归和版本对比基线

### 4.3 扩容预演

- `boards=20000`
- `points-per-board=4`
- 总点位数约 `80000`
- `rounds=5`
- 目的：在版本发布前观察扩容后查询波动

## 5. 当前关键查询范围

压测默认覆盖以下查询：

- `filterOptions.line_name`
- `filterOptions.program_name`
- `filterOptions.device_name`
- `summary.metrics`
- `summary.count`
- `summary.rows`
- `badPoint.total`
- `badPoint.rows`
- `grade.total`
- `grade.rows`
- `board.count`
- `board.rows`
- `point.count`
- `point.rows`

这些查询和正式仓储共用 [MySqlQueryBuilder.cpp](/D:/Program/spc/LaserSpc/src/infrastructure/MySqlQueryBuilder.cpp)，因此 benchmark SQL 与实际项目查询口径一致。

## 6. 当前验收标准

以 `boards=5000`、`points-per-board=4`、`rounds=5` 为基线，建议按以下标准验收：

### 6.1 功能性通过标准

- 压测工具能成功创建或复用 `laser_spc_perf`
- 造数完成后数据量符合预期
- benchmark 全部查询成功执行
- EXPLAIN 能正常输出，不出现驱动层报错
- 真实 MySQL 集成测试全部通过

### 6.2 性能通过标准

平均耗时建议控制在以下范围内：

- `filterOptions.*` 不高于 `3 ms`
- `summary.metrics` 不高于 `5 ms`
- `summary.rows` 不高于 `5 ms`
- `badPoint.rows` 不高于 `5 ms`
- `grade.rows` 不高于 `8 ms`
- `board.rows` 不高于 `5 ms`
- `point.rows` 不高于 `5 ms`

如果单项超过上述阈值，需要结合 EXPLAIN 和日志继续分析。

### 6.3 执行计划通过标准

- 关键主表查询不应出现无索引的 `type=ALL`
- 允许 `summary.count` 对派生小表出现 `ALL`，但不允许原始大表全表扫描
- `key` 应尽量命中新加的组合索引或已有时间索引
- 若 `Extra` 出现 `Using filesort`，需要确认是否是聚合或排序字段本身决定，不能盲目接受

## 7. 本轮索引优化结论

针对当前日志和压测结果，已补充以下索引：

### 7.1 board_records

- `idx_board_program_name (program_name)`
- `idx_board_device_name (device_name)`
- `idx_board_line_event (line_name, event_time)`
- `idx_board_result_line_event (result, line_name, event_time)`

### 7.2 point_records

- `idx_point_line_end_time (line_name, end_time)`
- `idx_point_result_line_end_time (result, line_name, end_time)`

索引来源：

- 新库初始化时由 [migrations](/D:/Program/spc/LaserSpc/sql/mysql/migrations) 下的迁移脚本直接建出
- [LaserSpcDbInit.cpp](/D:/Program/spc/LaserSpc/tools/LaserSpcDbInit.cpp) 已纳入这些迁移脚本
- `LaserSpcPerfTool` 也会在压测库上自动检查并补充这些索引

## 8. 推荐回归顺序

1. 运行 `LaserSpcDbInit.exe`，确认业务库结构和索引已补齐
2. 运行 MySQL smoke test
3. 运行完整 MySQL 集成测试
4. 运行 `LaserSpcPerfTool --mode all`
5. 对比本次 benchmark 与上个版本的基线

## 9. 结果记录建议

每次正式回归至少记录以下内容：

- Git 提交号
- Qt 版本
- MySQL 版本
- 压测参数
- benchmark 输出
- EXPLAIN 输出
- 是否满足当前验收标准

如果结果异常，优先附带以下信息：

- 对应查询名
- 平均耗时变化幅度
- 命中索引变化
- 是否新增 `Using temporary` / `Using filesort` / `type=ALL`
