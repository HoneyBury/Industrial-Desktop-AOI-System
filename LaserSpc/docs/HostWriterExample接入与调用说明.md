# Host Writer Example 接入与调用说明

本文专门说明 [`examples/host_spc_writer`](../examples/host_spc_writer) 这套示例代码如何在宿主程序中使用。

它不是 HTTP API 文档，而是“宿主程序直接链接 `LaserSpcCore/LaserSpcUi` 后，如何通过 C++ API 写入数据”的说明。

适用场景：

- 你的上位机本身就是 Qt/C++ 程序
- 你不想再走 `LaserSpcIngestServer` 的 HTTP 接口
- 你希望直接在进程内完成 SPC 数据写入
- 你需要“整板批量写入”“按点聚合后延迟写入”“同步/异步写入”这几种模式

## 1. 相关文件

`examples/host_spc_writer` 里最重要的是这 4 个文件：

- `SpcWriteManager.h / .cpp`
- `BoardBatchAggregator.h / .cpp`

它们分别负责：

- `SpcWriteManager`
  - 封装 `IngestService`
  - 对外提供同步写入和异步写入接口
  - 适合“已经拿到完整 batch，准备直接写库”的场景
- `BoardBatchAggregator`
  - 负责按 `boardCode` 缓存板级和点位数据
  - 当点位齐全或被显式标记完成时，再拼成 `InspectionBatch`
  - 适合“点位结果逐个到达，最后再统一落库”的场景

## 2. 数据模型

示例程序主要围绕下面 3 个领域对象工作：

```cpp
LaserSpc::Domain::BoardRecordRow
LaserSpc::Domain::PointRecordRow
LaserSpc::Domain::InspectionBatch
```

语义分别是：

- `BoardRecordRow`
  - 一块板的板级结果
- `PointRecordRow`
  - 一块板中的一个点位结果
- `InspectionBatch`
  - 一块板加它对应的全部点位

其中：

- `storeBatch()` / `enqueueBatch()` 的语义是“整板替换”
- 同一个 `boardCode` 再次写入 batch，会以最后一次成功提交为准
- `storePoint()` / `enqueuePoint()` 现在是按 `(boardCode, pointName)` 做幂等 upsert
- `BoardBatchAggregator` 不会 flush 空 batch；没有点位时只会继续缓存或发出 deferred 提示

## 3. 最小接入方式

如果你的宿主工程已经把 `LaserSpc` 作为子目录接入，最小写法如下：

```cmake
add_subdirectory(third_party/LaserSpc)

add_library(HostSpcWriterSupport STATIC
    src/SpcWriteManager.cpp
    src/BoardBatchAggregator.cpp
)
target_include_directories(HostSpcWriterSupport PUBLIC src)
target_link_libraries(HostSpcWriterSupport PUBLIC LaserSpcCore)
```

如果你直接复用仓库里的示例文件，需要确保：

- 目标开启 `AUTOMOC`
- 能包含 `LaserSpc/src`
- 已链接 `LaserSpcCore`

## 4. 初始化写入管理器

### 4.1 用整体配置初始化

```cpp
#include "SpcWriteManager.h"
#include "infrastructure/AppConfigService.h"

LaserSpc::Infrastructure::AppConfigService configService;
const auto settings = configService.settings();

HostSpc::SpcWriteManager writer(settings);
```

这段代码的含义：

- 从 ini / 环境变量读出数据库与 MES 配置
- 用同一套配置初始化写入器
- 后续所有 `store...()` / `enqueue...()` 都会使用这份配置

### 4.2 手工指定数据库和 MES 配置

```cpp
LaserSpc::Infrastructure::DatabaseSettings db;
db.host = "127.0.0.1";
db.port = 3306;
db.databaseName = "laser_spc";
db.userName = "laserspc";
db.password = "LaserSpc#2026";

LaserSpc::Infrastructure::MesSettings mes;

HostSpc::SpcWriteManager writer;
writer.initialize(db, mes);
```

适用于：

- 宿主程序有自己的配置页
- 你不想依赖默认配置文件

## 5. 整板批量插入

这是最推荐的接入方式。

前提是你的业务在一块板结束时，已经能拿到：

- 板级结果
- 这块板对应的全部点位结果

### 5.1 同步批量插入

```cpp
#include "SpcWriteManager.h"

LaserSpc::Domain::BoardRecordRow board;
board.boardCode = "BD-20260404-0001";
board.result = "NG";
board.lineName = "L1";
board.programName = "Program-A";
board.deviceName = "Laser-01";
board.operatorName = "Alice";
board.eventTime = QDateTime::currentDateTime();

LaserSpc::Domain::PointRecordRow point1;
point1.boardCode = board.boardCode;
point1.pointName = "P01";
point1.result = "OK";
point1.readGrade = "A";
point1.readCodeContent = "READ-P01";
point1.lineName = board.lineName;
point1.programName = board.programName;
point1.deviceName = board.deviceName;
point1.startTime = board.eventTime.addSecs(-3);
point1.endTime = board.eventTime.addSecs(-2);

LaserSpc::Domain::PointRecordRow point2 = point1;
point2.pointName = "P02";
point2.result = "NG";
point2.readGrade = "C";
point2.readCodeContent = "READ-P02";
point2.endTime = board.eventTime.addSecs(-1);

auto batch = HostSpc::SpcWriteManager::buildBatch(
    QString(),
    board,
    {point1, point2});

const LaserSpc::Domain::IngestResult result = writer.storeBatch(batch);
if (!result.success) {
    qWarning() << "storeBatch failed:" << result.errorMessage;
}
```

这段代码做了什么：

1. 先构造板级记录 `board`
2. 再构造多个点位记录 `point1/point2`
3. 调 `buildBatch()` 拼成一个 `InspectionBatch`
4. 调 `storeBatch()` 在当前线程直接写库

特点：

- 调用方线程会等待数据库和可能的 MES 转发完成
- 结果最直接，错误最好处理
- 适合低并发或已有专门后台线程的宿主程序

### 5.2 `buildBatch()` 会自动补的默认值

如果你传给点位的这些字段为空：

- `boardCode`
- `lineName`
- `programName`
- `deviceName`
- `startTime`
- `endTime`

`buildBatch()` / `storeBatch()` 会用板级信息补齐。

但业务上仍建议你显式填写清楚，原因是：

- 可读性更好
- 调试更直接
- 避免“字段被自动兜底但你自己没意识到”

## 6. 异步批量插入

如果你不希望当前业务线程被数据库写入阻塞，应使用异步模式。

### 6.1 基本调用

```cpp
HostSpc::SpcWriteManager writer(settings);

QObject::connect(
    &writer,
    &HostSpc::SpcWriteManager::batchStored,
    [](const QString& requestId, const LaserSpc::Domain::IngestResult& result) {
        if (!result.success) {
            qWarning() << "async batch failed:" << requestId << result.errorMessage;
            return;
        }
        qDebug() << "async batch stored:" << requestId
                 << "boards=" << result.insertedBoards
                 << "points=" << result.insertedPoints;
    });

writer.enqueueBatch(batch);
```

这段代码的含义：

- `enqueueBatch()` 会在需要时自动启动单独的写线程
- 调用线程不会被数据库写入阻塞
- 真正写完后，通过 `batchStored` 信号回调结果

### 6.2 适合什么场景

适合：

- 生产线程节拍敏感
- 你想把数据库写入串行化
- 你希望统一在一个 writer 线程里顺序落库

不适合：

- 当前逻辑必须立刻拿到写入结果再继续
- 你没有设计异步错误处理

### 6.3 异步批量插入时要注意什么

- 不要自己随便复用重复的 `requestId`
- 如果不传 `requestId`，内部会自动生成强唯一 ID
- `batchStored` 回调只代表这次 batch 已完成，不代表 UI 已刷新
- 析构或停机前如果调用 `stopAsyncWriter()`，会等待当前排队任务结束

## 7. 延迟插入 / 按点聚合后再写

这是 `BoardBatchAggregator` 的用途。

它适合这样的业务流：

- 先收到板级开始/结束信息
- 点位数据一条一条上报
- 点位齐了之后，再一起写成一块板

### 7.1 已知点位总数的延迟插入

```cpp
#include "BoardBatchAggregator.h"

HostSpc::SpcWriteManager writer(settings);
HostSpc::BoardBatchAggregator aggregator(&writer);

aggregator.setFlushMode(HostSpc::BoardBatchAggregator::FlushMode::Sync);

QObject::connect(
    &aggregator,
    &HostSpc::BoardBatchAggregator::batchFlushed,
    [](const QString& boardCode,
       const QString& requestId,
       const LaserSpc::Domain::IngestResult& result) {
        qDebug() << "board flushed:" << boardCode << requestId << result.success;
    });

LaserSpc::Domain::BoardRecordRow board;
board.boardCode = "BD-20260404-0002";
board.result = "OK";
board.lineName = "L1";
board.programName = "Program-A";
board.deviceName = "Laser-01";
board.operatorName = "Alice";
board.eventTime = QDateTime::currentDateTime();

aggregator.upsertBoard(board, "req-board-0002", 3);

aggregator.appendPoint(board.boardCode, point1);
aggregator.appendPoint(board.boardCode, point2);
aggregator.appendPoint(board.boardCode, point3);
```

这段代码的行为是：

- `upsertBoard(..., expectedPoints = 3)` 告诉聚合器这块板应有 3 个点
- 前两个点只缓存，不落库
- 第 3 个点 append 后自动 flush

### 7.2 未知点位总数，等板完成后再插入

```cpp
aggregator.upsertBoard(board, "req-board-0003");

aggregator.appendPoint(board.boardCode, point1);
aggregator.appendPoint(board.boardCode, point2);
aggregator.appendPoint(board.boardCode, point3);

aggregator.markBoardComplete(board.boardCode);
```

这段代码适合：

- 点位数量事先不知道
- 但你能明确收到“这块板结束了”的事件

`markBoardComplete()` 的作用：

- 把这块板标记为已完成
- 如果当前至少已有 1 个点位，则立即 flush

### 7.3 延迟插入但走异步写线程

```cpp
HostSpc::SpcWriteManager writer(settings);
HostSpc::BoardBatchAggregator aggregator(&writer);

aggregator.setFlushMode(HostSpc::BoardBatchAggregator::FlushMode::Async);

QObject::connect(
    &aggregator,
    &HostSpc::BoardBatchAggregator::batchPrepared,
    [](const QString& boardCode, const QString& requestId, int pointCount) {
        qDebug() << "batch prepared:" << boardCode << requestId << pointCount;
    });

QObject::connect(
    &aggregator,
    &HostSpc::BoardBatchAggregator::batchFlushed,
    [](const QString& boardCode,
       const QString& requestId,
       const LaserSpc::Domain::IngestResult& result) {
        qDebug() << "batch flushed:" << boardCode << requestId << result.success;
    });
```

这里分成了两层：

- 聚合层负责缓存、凑整板、决定何时 flush
- 写线程负责真正的数据库写入

这样适合：

- 产线线程只负责上报点位
- 落库统一在后台单线程完成

## 8. 强制 flush、超时 flush 和 deferred

`BoardBatchAggregator` 还提供了补偿接口：

```cpp
aggregator.flushBoard(boardCode);
aggregator.flushReadyBoards();
aggregator.flushExpired(5000);
aggregator.flushAll();
```

各自含义：

- `flushBoard(boardCode)`
  - 尝试强制 flush 某一块板
- `flushReadyBoards()`
  - 只 flush 已满足条件的板
- `flushExpired(maxPendingMs)`
  - flush 长时间未更新的板
- `flushAll()`
  - 扫描所有 pending 板并尝试 flush

注意：

- 空 batch 不会被真正 flush
- 如果板级信息存在，但点位为空，会触发 `batchDeferred`

示例：

```cpp
QObject::connect(
    &aggregator,
    &HostSpc::BoardBatchAggregator::batchDeferred,
    [](const QString& boardCode, const QString& reason) {
        qWarning() << "batch deferred:" << boardCode << reason;
    });
```

这个信号通常意味着：

- 当前板还没攒到任何点位
- 你不应该把它当成写入成功

## 9. 同步插入、异步插入、延迟插入怎么选

### 9.1 同步批量插入

优先使用条件：

- 已有完整整板数据
- 调用线程可以接受短暂阻塞
- 你希望错误处理最简单

推荐 API：

```cpp
writer.storeBatch(batch);
```

### 9.2 异步批量插入

优先使用条件：

- 已有完整整板数据
- 业务线程不希望被 DB/MES 阻塞

推荐 API：

```cpp
writer.enqueueBatch(batch);
```

说明：

- `startAsyncWriter()` 不是必须的前置调用
- `enqueueBatch()/enqueueBoard()/enqueuePoint()` 内部会自动启动写线程
- 如果你只是普通接入，直接 `enqueue...()` 即可
- 只有想提前预热线程时，才需要手动调用 `startAsyncWriter()`

### 9.3 延迟插入

优先使用条件：

- 点位是流式到达
- 要按板聚合后再统一写入

推荐 API：

```cpp
aggregator.upsertBoard(...)
aggregator.appendPoint(...)
aggregator.markBoardComplete(...)
```

### 9.4 同步延迟插入 vs 异步延迟插入

区别只在 flush 之后的落库位置：

- `FlushMode::Sync`
  - 聚合完成后，当前线程直接写
- `FlushMode::Async`
  - 聚合完成后，投递到 writer 线程写

## 10. 点位详情 JSON 的写法

如果你希望 `PointRecordPage` 能打开点位详情，建议在写点位前把详情文件一起落好。

示例程序里已经提供了一个静态 helper：

```cpp
LaserSpc::Domain::PointRecordRow row;
row.boardCode = "BD-20260404-0004";
row.pointName = "P01";
row.result = "OK";
row.readGrade = "A";
row.readCodeContent = "READ-P01";
row.lineName = "L1";
row.programName = "Program-A";
row.deviceName = "Laser-01";
row.startTime = QDateTime::currentDateTime().addSecs(-2);
row.endTime = QDateTime::currentDateTime();

QString errorMessage;
if (!HostSpc::HostSpcExampleWindow::ensurePointDetailFile(&row, &errorMessage)) {
    qWarning() << "save detail json failed:" << errorMessage;
}
```

调用后会发生两件事：

1. 自动生成 `detailJsonPath`
2. 把点位详情 JSON 写到对应文件

然后你再把这个 `row` 放进 `storePoint()` 或 `storeBatch()` 即可。

当前版本补充说明：

- 同步接口 `storeBatch()/storePoint()` 会先落 JSON，再继续写数据库
- 异步接口 `enqueueBatch()/enqueuePoint()` 仍然沿用原有单写线程流程，不额外拆分新的 JSON 异步存储层
- 点位详情保存目录现在可以在“系统设置”里配置；留空时使用默认目录 `<程序目录>/point_details`
- `PointRecordPage` 的点位详情弹窗已经改成中文摘要视图，常用字段会直接展示为：
  - 模板文件
  - 程序名
  - 识别结果
  - 开始时间
  - 结束时间
  - 镭射内容
  - 读码内容
  - 原始 JSON 树中的字段名也会自动转成中文

## 11. 常用信号说明

### `SpcWriteManager`

```cpp
batchStored(QString requestId, IngestResult result)
boardStored(QString boardCode, IngestResult result)
pointStored(QString boardCode, QString pointName, IngestResult result)
```

适用时机：

- 你自己直接使用 `enqueueBatch / enqueueBoard / enqueuePoint`

### `BoardBatchAggregator`

```cpp
batchPrepared(QString boardCode, QString requestId, int pointCount)
batchFlushed(QString boardCode, QString requestId, IngestResult result)
batchDeferred(QString boardCode, QString reason)
```

适用时机：

- 你使用了聚合器
- 想区分“已聚合完成”和“已写库完成”这两个阶段

## 12. 推荐代码模板

### 模板 A：最常见的整板同步写入

```cpp
HostSpc::SpcWriteManager writer(settings);

auto batch = HostSpc::SpcWriteManager::buildBatch(
    QString(),
    board,
    points);

const auto result = writer.storeBatch(batch);
if (!result.success) {
    qWarning() << result.errorMessage;
}
```

### 模板 B：整板异步写入

```cpp
HostSpc::SpcWriteManager writer(settings);

QObject::connect(&writer, &HostSpc::SpcWriteManager::batchStored,
                 [](const QString& requestId, const LaserSpc::Domain::IngestResult& result) {
    if (!result.success) {
        qWarning() << "failed:" << requestId << result.errorMessage;
    }
});

writer.enqueueBatch(batch);
```

### 模板 C：点位流式到达，最终延迟写入

```cpp
HostSpc::SpcWriteManager writer(settings);
HostSpc::BoardBatchAggregator aggregator(&writer);

aggregator.setFlushMode(HostSpc::BoardBatchAggregator::FlushMode::Async);

aggregator.upsertBoard(board, QString(), expectedPoints);

for (const auto& point : incomingPoints) {
    aggregator.appendPoint(board.boardCode, point);
}
```

如果事先不知道点位总数，则改为：

```cpp
aggregator.upsertBoard(board);

for (const auto& point : incomingPoints) {
    aggregator.appendPoint(board.boardCode, point);
}

aggregator.markBoardComplete(board.boardCode);
```

## 13. 最后建议

如果你的业务允许，优先级建议如下：

1. 优先 `storeBatch()` / `enqueueBatch()`
2. 点位流式上报时，再使用 `BoardBatchAggregator`
3. 单点 `storePoint()` 只用于确实无法组成整板 batch 的场景

原因很简单：

- `batch` 语义最接近当前系统的真实业务模型
- 更容易保证数据完整性
- 更容易回放和排查问题
- 与当前查询和展示模型更一致

## 14. 相关文档

- [`docs/上位机客户端嵌入说明.md`](./上位机客户端嵌入说明.md)
- [`docs/数据存储与API调用说明.md`](./数据存储与API调用说明.md)
- [`examples/host_spc_writer/README.md`](../examples/host_spc_writer/README.md)
