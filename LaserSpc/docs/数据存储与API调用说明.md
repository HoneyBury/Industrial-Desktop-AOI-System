# 数据存储与 API 调用说明

## 1. 当前项目的数据读写结构

### 1.1 查询链路

界面层不直接访问数据库，统一走下面这条链路：

`MainWindow / Page -> AppServiceFacade -> QueryService -> ISpcQueryRepository -> MySqlSpcRepository / MockSpcRepository`

对应代码位置：

- `src/ui/shell/MainWindow.cpp`
- `src/app/AppServiceFacade.cpp`
- `src/domain/Repository.h`
- `src/infrastructure/RepositoryFactory.cpp`

说明：

- `MainWindow` 和各页面只负责组装筛选条件、分页和排序参数。
- `AppServiceFacade` 负责创建 `SummaryQueryService`、`RecordQueryService`、`StatQueryService`。
- 具体数据源由 `RepositoryFactory::build()` 决定。
- 当 `settings.useMySql = true` 且 MySQL 可用时，走 `MySqlSpcRepository`。
- 否则自动回退到 `MockSpcRepository`。

### 1.2 写入链路

写入不是只有一种方式，当前项目有两条正式链路：

1. `HTTP Client -> LaserSpcIngestServer -> IngestService -> ISpcWriteRepository -> MySqlSpcWriteRepository`
2. `Host App -> SpcWriteManager -> IngestService -> ISpcWriteRepository -> MySqlSpcWriteRepository`

对应代码位置：

- `tools/LaserSpcIngestServer.cpp`
- `examples/host_spc_writer/SpcWriteManager.cpp`
- `src/app/IngestService.cpp`
- `src/domain/CommandRepository.h`
- `src/infrastructure/MySqlSpcWriteRepository.cpp`

说明：

- 上游设备、MES 或中间服务可以走 HTTP 接口。
- 嵌入到上位机或宿主 Qt 工程时，可以直接调用 `SpcWriteManager::storeBatch()/enqueueBatch()/storePoint()/enqueuePoint()`。
- `IngestService` 负责字段校验、默认值补齐、批次逻辑和 MES 转发。
- `MySqlSpcWriteRepository` 负责真正的 MySQL 插入、更新和事务处理。
- 点位详情 JSON 仍保存在文件系统，不保存在 MySQL；当前写入链路会在数据库提交前先完成 JSON 文件落盘。
- 对嵌入式直调链路，若宿主未传 `laserContent`，系统会自动补成 `<pointName>-LASER` 后再写 JSON 和数据库。

## 2. 推荐的数据接入方式

如果是“设备/产线往 SPC 系统写数据”，推荐优先在下面两种方式中二选一，不要让设备端直接连数据库。

1. 独立部署时，走 `LaserSpcIngestServer` 的 HTTP API
2. 嵌入到现有 Qt 上位机时，直接调用 `SpcWriteManager`

原因：

- 设备端和数据库解耦，后续改库表或鉴权方式时影响更小。
- 服务端统一做字段校验，避免脏数据直接入库。
- `inspection-batches` 支持“单板 + 点位”一次性事务写入，最适合实际产线。
- 后续如果要接 MQ、重试、审计日志，也可以继续在服务端扩展。

## 3. API 服务启动方式

`LaserSpcIngestServer` 默认监听：

```text
http://<host>:8099
```

关键环境变量：

```text
LASERSPC_API_HOST=0.0.0.0
LASERSPC_API_PORT=8099
LASERSPC_API_KEY=replace-with-your-token
LASERSPC_SYSTEM_SETTINGS_ENABLED=1
LASERSPC_EXPORT_REPORT_ENABLED=1
LASERSPC_DB_HOST=127.0.0.1
LASERSPC_DB_PORT=3306
LASERSPC_DB_NAME=laser_spc
LASERSPC_DB_USER=laserspc
LASERSPC_DB_PASSWORD=LaserSpc#2026
```

说明：

- `LASERSPC_API_KEY` 为空时，`POST` 接口不校验鉴权。
- 配置了 `LASERSPC_API_KEY` 后，请在请求头里带上 `X-API-Key`。
- `LASERSPC_SYSTEM_SETTINGS_ENABLED` 控制系统设置入口默认是否可用。
- `LASERSPC_EXPORT_REPORT_ENABLED` 控制 CSV / 截图 / 诊断报告导出是否可用。

## 4. 可调用的写入 API

### 4.1 健康检查

```http
GET /health
```

用途：

- 用于部署完成后的可用性检查。
- 不需要请求体。

### 4.2 写入单板记录

```http
POST /api/v1/board-records
X-API-Key: <token>
Content-Type: application/json
```

请求体示例：

```json
{
  "boardCode": "BD-240301-0101",
  "result": "OK",
  "lineName": "L1",
  "programName": "Program-A",
  "deviceName": "Laser-01",
  "operatorName": "Alice",
  "eventTime": "2026-03-16T10:15:30"
}
```

处理逻辑：

- 调用 `IngestService::ingestBoardRecord()`
- 再调用 `ISpcWriteRepository::upsertBoardRecord()`
- 语义是 upsert，按 `board_code` 更新或插入

适用场景：

- 只需要同步板级结果，不需要点位明细

### 4.3 写入单点记录

```http
POST /api/v1/point-records
X-API-Key: <token>
Content-Type: application/json
```

请求体示例：

```json
{
  "boardCode": "BD-240301-0101",
  "pointName": "Code-A1",
  "result": "OK",
  "readGrade": "A",
  "laserContent": "Code-A1-LASER",
  "readCodeContent": "READ-Code-A1",
  "isLaser": true,
  "isReadCode": true,
  "lineName": "L1",
  "programName": "Program-A",
  "deviceName": "Laser-01",
  "startTime": "2026-03-16T10:15:00",
  "endTime": "2026-03-16T10:15:30"
}
```

处理逻辑：

- 调用 `IngestService::ingestPointRecord()`
- 再调用 `ISpcWriteRepository::insertPointRecord()`
- 语义是按 `(boardCode, pointName)` 做单点幂等 upsert
- 如果 `readGrade` 为空，服务端会自动写成 `"null"`
- 如果 `readCodeContent` 为空，服务端会自动写成 `"null"`
- 如果 `isLaser / isReadCode` 未传，服务端默认写 `false`

适用场景：

- 点位结果独立上传
- 板级记录和点位记录不是同一时刻产生

### 4.4 批次写入

```http
POST /api/v1/inspection-batches
X-API-Key: <token>
Content-Type: application/json
```

请求体示例：

```json
{
  "requestId": "batch-20260316-001",
  "board": {
    "boardCode": "BD-240301-0101",
    "result": "NG",
    "lineName": "L1",
    "programName": "Program-A",
    "deviceName": "Laser-01",
    "operatorName": "Alice",
    "eventTime": "2026-03-16T10:15:30"
  },
  "points": [
    {
      "pointName": "Code-A1",
      "result": "OK",
      "readGrade": "A",
      "laserContent": "Code-A1-LASER",
      "readCodeContent": "READ-Code-A1",
      "isLaser": true,
      "isReadCode": true,
      "startTime": "2026-03-16T10:15:00",
      "endTime": "2026-03-16T10:15:10"
    },
    {
      "pointName": "Code-A2",
      "result": "NG",
      "readGrade": "C",
      "startTime": "2026-03-16T10:15:11",
      "endTime": "2026-03-16T10:15:30"
    }
  ]
}
```

处理逻辑：

1. `IngestService` 先校验 `board`
2. 自动把 `boardCode/lineName/programName/deviceName` 补给 points 中缺失的字段
3. 调用 `replaceInspectionBatch(board, points)`
4. 服务端会先 upsert 单板，再删除该板旧点位，再插入新点位
5. 整个过程应作为一笔事务执行
6. 如果当前 MES 开启，还会在数据库成功后转发给 MES

补充：

- 当前 MySQL 提交使用显式 SQL 事务语句，而不是 Qt SQL 的事务 API
- 如果点位包含 `detailJsonPath` 或由接入层自动生成详情路径，系统会先写 JSON 文件，再执行数据库事务
- 点位详情目录可以通过系统设置或环境变量 `LASERSPC_POINT_DETAIL_DIR` 调整

这是最推荐的生产接入接口。

## 5. 实际调用示例

### 5.1 curl

```bash
curl -X POST "http://127.0.0.1:8099/api/v1/inspection-batches" \
  -H "Content-Type: application/json" \
  -H "X-API-Key: replace-with-your-token" \
  -d '{
    "requestId": "batch-20260323-001",
    "board": {
      "boardCode": "BD-20260323-0001",
      "result": "OK",
      "lineName": "L1",
      "programName": "Program-A",
      "deviceName": "Laser-01",
      "operatorName": "Alice",
      "eventTime": "2026-03-23T08:30:00"
    },
    "points": [
      {
        "pointName": "P01",
        "result": "OK",
        "readGrade": "A",
        "startTime": "2026-03-23T08:29:58",
        "endTime": "2026-03-23T08:29:59"
      }
    ]
  }'
```

### 5.2 Qt 客户端调用

```cpp
QNetworkRequest request(QUrl("http://127.0.0.1:8099/api/v1/inspection-batches"));
request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
request.setRawHeader("X-API-Key", "replace-with-your-token");

QJsonObject board{
    {"boardCode", "BD-20260323-0001"},
    {"result", "OK"},
    {"lineName", "L1"},
    {"programName", "Program-A"},
    {"deviceName", "Laser-01"},
    {"operatorName", "Alice"},
    {"eventTime", "2026-03-23T08:30:00"}
};

QJsonArray points;
points.append(QJsonObject{
    {"pointName", "P01"},
    {"result", "OK"},
    {"readGrade", "A"},
    {"startTime", "2026-03-23T08:29:58"},
    {"endTime", "2026-03-23T08:29:59"}
});

QJsonObject payload{
    {"requestId", "batch-20260323-001"},
    {"board", board},
    {"points", points}
};

networkAccessManager->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
```

## 6. 接口返回说明

典型返回字段：

```json
{
  "success": true,
  "insertedBoards": 1,
  "insertedPoints": 2,
  "requestId": "batch-20260323-001",
  "forwardMessage": "MES forwarded.",
  "error": ""
}
```

字段说明：

- `success`: 是否成功
- `insertedBoards`: 影响的单板记录数
- `insertedPoints`: 影响的点位记录数
- `requestId`: 批次接口原样返回
- `forwardMessage`: MES 转发结果
- `error`: 失败原因

## 7. 功能开关与查重 API

### 7.1 系统设置开关

```http
GET /api/v1/settings-availability
POST /api/v1/settings-availability
X-API-Key: <token>
Content-Type: application/json
```

`POST` 请求体：

```json
{
  "enabled": false
}
```

用途：

- `enabled=true` 时允许客户端打开系统设置入口
- `enabled=false` 时客户端应禁用系统设置入口

### 7.2 导出报表开关

```http
GET /api/v1/export-report-availability
POST /api/v1/export-report-availability
X-API-Key: <token>
Content-Type: application/json
```

`POST` 请求体：

```json
{
  "enabled": false
}
```

用途：

- 控制 CSV、截图和诊断报告导出是否允许使用

### 7.3 镭射内容查重

```http
GET /api/v1/laser-content-duplicates?laserContent=Code-A1-LASER
X-API-Key: <token>
```

返回示例：

```json
{
  "success": true,
  "laserContent": "Code-A1-LASER",
  "exists": true,
  "duplicateCount": 2,
  "latestBoardCode": "BD-240301-0102",
  "latestPointName": "Code-A1",
  "latestEndTime": "2026-03-16T10:15:30"
}
```

说明：

- `exists=true` 表示数据库中已经存过该镭射内容
- `duplicateCount` 是当前相同 `laser_content` 的总条数
- `latestBoardCode / latestPointName / latestEndTime` 返回最近一条命中记录

## 8. 应该如何在项目里调用对应 API

### 7.1 如果你是“设备端/采集端”

直接调 HTTP 写入接口：

- 优先使用 `POST /api/v1/inspection-batches`
- 次选 `POST /api/v1/board-records` 和 `POST /api/v1/point-records`

不要直接操作 MySQL。

### 7.2 如果你是“SPC 看板客户端”

不要调写入 API，继续走现有查询仓储：

- `SummaryPage` 使用 `summaryQueryService()`
- `BadStatPage` 使用 `statQueryService()`
- `BoardRecordPage` 和 `PointRecordPage` 使用 `recordQueryService()`

### 7.3 如果你要新增一个中间服务

建议这样分层：

1. 中间服务接收设备原始报文
2. 转成 `inspection-batches` 所需 JSON
3. 调用 `LaserSpcIngestServer`
4. SPC 看板继续从 MySQL 查询

这样可以把设备协议适配和 SPC 存储彻底分开。

## 9. 建议结论

当前项目最合适的实际落地方案是：

- 查询：继续保留 `RepositoryFactory -> MySqlSpcRepository`
- 独立服务接入：优先由 `LaserSpcIngestServer` 暴露 HTTP API
- 宿主嵌入接入：优先直接调用 `SpcWriteManager`，尤其是 `enqueueBatch()` / `enqueuePoint()`
- 生产接入：批量场景优先 `inspection-batches` 或 `SpcWriteManager::buildBatch() + enqueueBatch()`

如果后续需要，我建议下一步再补两项：

- 一份“库表字段与 API 字段映射表”
- 一份“设备接入的错误码与重试策略说明”
