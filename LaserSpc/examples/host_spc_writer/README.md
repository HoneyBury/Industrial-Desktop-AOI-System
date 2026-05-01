# Host SPC Writer Template

这个目录放的是“宿主程序直接链接 `LaserSpcCore/LaserSpcUi`，不走 HTTP”的写入模板。

包含文件：

- `SpcWriteManager.h`
- `SpcWriteManager.cpp`
- `BoardBatchAggregator.h`
- `BoardBatchAggregator.cpp`
- `HostSpcExampleWindow.h`
- `HostSpcExampleWindow.cpp`
- `main.cpp`
- `CMakeLists.txt`

设计目标：

- 不把 `IngestService` 直接暴露给宿主业务层
- 支持生产线程里同步写入
- 支持单独写线程异步写入
- 支持按 `boardCode` 聚合点位，等整板完成后一次提交
- 提供一个仓库内可直接运行的宿主示例窗口

## 0. 直接运行示例

这个目录现在会生成两个目标：

- `LaserSpcHostWriterSupport`
  - 可复用的宿主写入支持库
- `LaserSpcHostWriterExample`
  - 左侧嵌入 `DashboardWidget`，右侧提供写入控制台的示例程序

根工程默认开启这个示例，可通过 `LASERSPC_BUILD_HOST_WRITER_EXAMPLE` 控制：

```bash
cmake -S . -B build -DLASERSPC_BUILD_HOST_WRITER_EXAMPLE=ON
cmake --build build --target LaserSpcHostWriterExample
```

示例窗口用途：

- 直接验证 `LaserSpcUi` 嵌入宿主界面
- 直接验证同步批量写入
- 直接验证异步单写线程
- 直接验证 `BoardBatchAggregator` 的缓冲与 flush 逻辑
- 直接验证 MES 调试窗口与本地回调接收

## 1. 文件职责

### 1.1 `SpcWriteManager`

负责两件事：

- 组装 `MySqlSpcWriteRepository + IngestService + MesEventForwarder`
- 对外提供同步写和异步写接口

建议用法：

- 如果你已经在生产线程里写入：优先直接用同步接口 `storeBatch()`
- 如果你不希望业务线程被数据库和 MES 阻塞：直接用 `enqueueBatch()` / `enqueueBoard()` / `enqueuePoint()`；这些接口内部会自动启动异步写线程

### 1.2 `BoardBatchAggregator`

负责把同一块板的数据聚合成一个 `InspectionBatch`。

它支持三种 flush 方式：

- 已知点位总数：达到 `expectedPoints` 自动 flush
- 不知道点位总数：调用 `markBoardComplete()` flush
- 超时或停机兜底：调用 `flushExpired()` 或 `flushAll()`

## 2. 最推荐的业务流程

### 2.1 一块板数据齐全后一次写入

如果你的设备或业务逻辑在“单板结束”时能拿到整块板结果，最简单：

1. 构造 `BoardRecordRow`
2. 构造多个 `PointRecordRow`
3. 调 `SpcWriteManager::buildBatch()`
4. 调 `storeBatch()` 或 `enqueueBatch()`

`PointRecordRow` 新增字段：

- `isLaser`：是否镭射
- `isReadCode`：是否读码

这是最贴近当前 `LaserSpc` 业务语义的接法。

### 2.2 点位流式到达时先聚合再落库

如果你拿到的是逐点结果流：

1. 收到板级信息时调用 `upsertBoard()`
2. 每收到一个点位调用 `appendPoint()`
3. 如果知道总点数，传 `expectedPoints`
4. 如果不知道总点数，整板完成时调用 `markBoardComplete()`

## 3. 典型用法

### 3.0 最短接入说明

同步写：

```cpp
HostSpc::SpcWriteManager writer(appSettings);
const auto result = writer.storeBatch(batch);
```

异步写：

```cpp
auto* writer = new HostSpc::SpcWriteManager(appSettings, this);

connect(writer, &HostSpc::SpcWriteManager::batchStored,
        this, [](const QString& requestId, const LaserSpc::Domain::IngestResult& result) {
    Q_UNUSED(requestId);
    Q_UNUSED(result);
});

writer->enqueueBatch(batch);
```

最重要的 3 个结论：

- `enqueueBatch()/enqueueBoard()/enqueuePoint()` 内部会自动启动异步写线程
- 同步和异步模式都会真实写入 JSON 文件，然后再继续写数据库
- 异步 `SpcWriteManager` 不能是“函数里的临时局部变量”，否则函数返回时对象析构，线程会被停止

### 3.1 同步写

```cpp
HostSpc::SpcWriteManager writer(appSettings);

auto batch = HostSpc::SpcWriteManager::buildBatch(
    "req-001",
    boardRow,
    {point1, point2, point3});

const auto result = writer.storeBatch(batch);
```

返回结果说明：

- `result.success = true`：数据库写入成功，若开启 MES，会继续附带 MES 转发结果
- `result.success = false`：本次写入失败，详细原因在 `result.errorMessage`
- `result.forwardMessage`：附加信息，例如 MES 转发结果或 JSON 明细文件告警

适用场景：

- 当前就在生产线程里写
- 数据量不大
- 不需要额外排队和削峰

### 3.2 异步单写线程

```cpp
HostSpc::SpcWriteManager writer(appSettings);

QObject::connect(&writer, &HostSpc::SpcWriteManager::batchStored,
                 [](const QString& requestId, const LaserSpc::Domain::IngestResult& result) {
    Q_UNUSED(requestId);
    Q_UNUSED(result);
});

writer.enqueueBatch(batch);
```

异步行为说明：

- `enqueueBatch()/enqueueBoard()/enqueuePoint()` 内部会自动调用 `startAsyncWriter()`，通常不需要你手动先调一次
- 这些接口只代表“已经入队”，不代表已经写库成功
- 真正的写入结果通过以下信号回调返回：
  - `batchStored(QString requestId, IngestResult result)`
  - `boardStored(QString boardCode, IngestResult result)`
  - `pointStored(QString boardCode, QString pointName, IngestResult result)`
- 如果你需要“宿主线程不阻塞”，优先用异步接口
- 如果你需要“当前线程立即知道结果”，用同步接口 `storeBatch()/storeBoard()/storePoint()`
- `startAsyncWriter()` 仍然保留，但更适合当作“预热线程”的可选接口，而不是固定前置步骤

### 3.3 异步 writer 的作用域要求

下面这种写法有风险：

```cpp
void submitOnce(const LaserSpc::Domain::InspectionBatch& batch) {
    HostSpc::SpcWriteManager writer(appSettings);
    writer.enqueueBatch(batch);
}
```

原因：

- `enqueueBatch()` 只是把任务投递到异步线程
- 如果函数马上返回，局部变量 `writer` 会立刻析构
- `SpcWriteManager::~SpcWriteManager()` 会调用 `stopAsyncWriter()`
- 这会导致异步线程被提前停止，后续任务可能来不及执行完

正确做法：

- 把 `SpcWriteManager` 放成宿主窗口成员、控制器成员，或者长期存活的单例
- 保证它的生命周期覆盖整个异步写入过程

推荐写法：

```cpp
class MyController : public QObject {
    Q_OBJECT
public:
    explicit MyController(QObject* parent = nullptr)
        : QObject(parent),
          m_writer(new HostSpc::SpcWriteManager(appSettings, this)) {
        connect(m_writer, &HostSpc::SpcWriteManager::batchStored,
                this, &MyController::handleBatchStored);
    }

    void submit(const LaserSpc::Domain::InspectionBatch& batch) {
        m_writer->enqueueBatch(batch);
    }

private:
    HostSpc::SpcWriteManager* m_writer = nullptr;
};
```

补充：

- 如果你只是偶发写一次，并且函数返回前必须拿到结果，直接用同步接口更合适
- 如果你确定要异步写，就不要把 writer 放在临时局部变量里

适用场景：

- 不希望生产线程被数据库或 MES 阻塞
- 想把写入顺序固定成单线程串行

### 3.4 聚合后自动 flush

```cpp
HostSpc::SpcWriteManager writer(appSettings);
HostSpc::BoardBatchAggregator aggregator(&writer);

aggregator.setFlushMode(HostSpc::BoardBatchAggregator::FlushMode::Async);

aggregator.upsertBoard(boardRow, "req-001", 3);
aggregator.appendPoint(boardRow.boardCode, point1);
aggregator.appendPoint(boardRow.boardCode, point2);
aggregator.appendPoint(boardRow.boardCode, point3);
```

上面这个例子里，第三个点到达后会自动 flush。

如果你拿不到总点数，改成：

```cpp
aggregator.upsertBoard(boardRow);
aggregator.appendPoint(boardRow.boardCode, point1);
aggregator.appendPoint(boardRow.boardCode, point2);
aggregator.markBoardComplete(boardRow.boardCode);
```

### 3.5 点位详情 JSON 存储说明（支持同步/异步）

示例里点位详情 JSON 由 `SpcWriteManager` 在写入链路中统一处理：

1. 业务侧构造 `PointRecordRow`（包含 `isLaser / isReadCode`）
2. `SpcWriteManager` 在真正入库前调用：
   - `PointDetailJsonService::buildDefaultFilePath()` 生成路径（若未指定）
   - `PointDetailJsonService::saveDetail()` 真正落盘 JSON
3. 然后再执行数据库写入

路径规则：

- 默认目录：`<应用目录>/point_details`
- 可用环境变量覆盖：`LASERSPC_POINT_DETAIL_DIR`
- 默认文件名：`{boardCode}_{pointName}_{yyyyMMdd_HHmmss}.json`

当前 JSON 关键字段包括：

- `templateFilePath`
- `laserTemplatePath`
- `laserContent`
- `readCodeContent`
- `success`
- `programName`
- `startTime`
- `endTime`
- `fieldDisplayNames`
- `算法规划`

当前版本还支持通过 `PointRecordRow::detail.extraFields` 写入任意额外字段。也就是说：

- 与数据库字段完全相同的内容，继续放在 `PointRecordRow` 本身
- 只需要落到详情 JSON、不需要进数据库的扩展字段，放到 `PointRecordRow::detail.extraFields`
- 如果这些扩展字段在 UI 上需要显示成中文名，放到 `PointRecordRow::detail.fieldDisplayNames`
- 业务上固定叫“算法规划”的内容，放到 `PointRecordRow::detail.algorithmPlan`

说明：

- `templateFilePath` 是当前推荐使用的模板文件路径字段
- `laserTemplatePath` 继续保留，用于兼容旧版本 JSON
- UI 会优先显示 `templateFilePath`，若没有则回退显示 `laserTemplatePath`
- `fieldDisplayNames` 用于定义“字段 key -> UI 显示名”的映射
- UI 显示时优先顺序是：业务传入的 `fieldDisplayNames` -> 内置字段中文映射 -> 原始 key
- `算法规划` 在 JSON 中按 `QJsonArray` 写入，数组长度不固定
- 读取旧数据时，如果历史 JSON 用的是 `algorithmPlan`，当前版本也会兼容读取

同步/异步行为：

- `Flush Mode = Sync`：JSON 与数据库都在当前线程执行
- `Flush Mode = Async`：JSON 与数据库都在单写线程执行（对业务线程异步）

结论：

- 同步写不是“只生成 JSON 路径”，而是会真实写入 JSON 文件，再继续写数据库
- 异步写也不是“只排队数据库”，而是会在异步写线程里先真实写入 JSON 文件，再继续写数据库
- 点位详情目录可以通过系统设置或环境变量 `LASERSPC_POINT_DETAIL_DIR` 覆盖

JSON 失败时当前版本会记录详细日志，常见失败原因包括：

- `reason=empty_path`
  - 业务侧没有提供路径，且默认路径构造失败
- `reason=mkpath_failed`
  - 目标目录没有权限创建，或者磁盘路径不可用
- `reason=open_failed`
  - 文件无法打开，常见于路径非法、目录权限不足、文件被占用
- `reason=commit_failed`
  - `QSaveFile` 提交失败，常见于磁盘权限、剩余空间不足、杀软占用
- `reason=parse_failed`
  - JSON 文件存在，但内容不是合法 JSON
- `reason=invalid_payload`
  - JSON 结构存在，但字段格式不符合读取要求

日志关键字：

- `PointDetailJson.save success`
- `PointDetailJson.save failed`
- `PointDetailJson.load success`
- `PointDetailJson.load failed`

这些日志会带上：

- 请求文件路径
- 解析后的绝对路径
- 程序名 / 时间范围
- 失败原因分类
- Qt 返回的底层错误文本

说明：

- 同步模式下，JSON 失败会在当前调用栈直接返回到 `IngestResult.forwardMessage`
- 异步模式下，JSON 失败会在异步写线程里记录日志，并通过 `batchStored/pointStored` 的 `forwardMessage` 回传

### 3.5.1 推荐写法

现在推荐的写法是业务侧直接填 `PointRecordRow::detail`：

```cpp
LaserSpc::Domain::PointRecordRow row;
row.boardCode = "BD-001";
row.pointName = "Code-01";
row.result = "OK";
row.readGrade = "A";
row.laserContent = "Code-01-LASER";
row.readCodeContent = "BD-001-Code-01";
row.lineName = "L1";
row.programName = "Program-A";
row.deviceName = "Laser-01";
row.startTime = QDateTime::currentDateTimeUtc().addSecs(-1);
row.endTime = QDateTime::currentDateTimeUtc();

// 固定摘要字段
row.detail.laserTemplatePath = "templates/Program-A.tpl";

// 任意扩展字段：只写 JSON，不进数据库
row.detail.extraFields.insert("templateRevision", "rev-03");
row.detail.extraFields.insert("cameraProfile", "LineScan-2");
row.detail.extraFields.insert("exposureMs", 12);
row.detail.extraFields.insert("templateFilePath", "templates/debug/Program-A/Code-01.tpl");

// 扩展字段显示名映射：控制详情弹窗里的中文名称
row.detail.fieldDisplayNames.insert("templateRevision", "模板版本");
row.detail.fieldDisplayNames.insert("cameraProfile", "相机方案");
row.detail.fieldDisplayNames.insert("exposureMs", "曝光时间(ms)");
row.detail.fieldDisplayNames.insert("roi", "区域范围");
row.detail.fieldDisplayNames.insert("x", "X坐标");
row.detail.fieldDisplayNames.insert("y", "Y坐标");
row.detail.fieldDisplayNames.insert("width", "宽度");
row.detail.fieldDisplayNames.insert("height", "高度");

QJsonObject roi;
roi.insert("x", 120);
roi.insert("y", 48);
roi.insert("width", 160);
roi.insert("height", 40);
row.detail.extraFields.insert("roi", roi);

// 算法规划：数组长度可变
row.detail.algorithmPlan = QJsonArray{
    QJsonObject{{"name", "定位"}, {"ok", "进入解码"}, {"ng", "输出定位失败"}},
    QJsonObject{{"name", "解码"}, {"ok", "进入质量判定"}, {"ng", "输出解码失败"}},
    QJsonObject{{"name", "质量判定"}, {"ok", "判定OK"}, {"ng", "判定NG"}}
};
row.detail.fieldDisplayNames.insert("name", "节点名称");
row.detail.fieldDisplayNames.insert("ok", "成功分支");
row.detail.fieldDisplayNames.insert("ng", "失败分支");
```

写入时 `SpcWriteManager` 会自动做这些事：

- 如果 `detailJsonPath` 为空，自动生成默认路径
- 如果 `detail.laserTemplatePath` 为空，但 `extraFields["templateFilePath"]` 有值，优先用它
- 如果 `detail` 里的固定字段为空，会回退使用 `PointRecordRow` 里的基础字段补齐
- 如果传了 `detail.fieldDisplayNames`，会一起写入 JSON，供详情 UI 渲染时使用
- 最终统一调用 `PointDetailJsonService::saveDetail()` 落盘

生成出来的 JSON 结构大致如下：

```json
{
  "version": 2,
  "data": {
    "templateFilePath": "templates/debug/Program-A/Code-01.tpl",
    "laserTemplatePath": "templates/debug/Program-A/Code-01.tpl",
    "laserContent": "Code-01-LASER",
    "readCodeContent": "BD-001-Code-01",
    "success": true,
    "programName": "Program-A",
    "startTime": "2026-04-24T12:00:00Z",
    "endTime": "2026-04-24T12:00:01Z",
    "templateRevision": "rev-03",
    "cameraProfile": "LineScan-2",
    "exposureMs": 12,
    "fieldDisplayNames": {
      "templateRevision": "模板版本",
      "cameraProfile": "相机方案",
      "exposureMs": "曝光时间(ms)",
      "roi": "区域范围",
      "x": "X坐标",
      "y": "Y坐标",
      "width": "宽度",
      "height": "高度",
      "name": "节点名称",
      "ok": "成功分支",
      "ng": "失败分支"
    },
    "roi": {
      "x": 120,
      "y": 48,
      "width": 160,
      "height": 40
    },
    "算法规划": [
      {"name": "定位", "ok": "进入解码", "ng": "输出定位失败"},
      {"name": "解码", "ok": "进入质量判定", "ng": "输出解码失败"},
      {"name": "质量判定", "ok": "判定OK", "ng": "判定NG"}
    ]
  }
}
```

UI 展示规则：

- 顶部摘要区固定显示：模板文件、程序名、识别结果、开始时间、结束时间、镭射内容、读码内容、算法规划摘要
- JSON 树会按 `fieldDisplayNames` 渲染扩展字段中文名
- `fieldDisplayNames` 本身不会单独显示成一条业务数据
- JSON 树完整显示 `data` 下的所有其他扩展字段
- `算法规划` 会显示数组项数量，并展开每个节点对象

## 3.5 数据库写入日志说明（同步/异步通用）

当前写库链路的日志分成 3 层：

1. 连接层：`DatabaseConnection`
2. SQL 执行层：`MySqlSpcWriteRepository`
3. 写入调度层：`SpcWriteManager / SpcWriteWorker`

### 连接层日志

成功时：

- `MySQL connection opened`

失败时会尽量分类：

- `reason=driver_unavailable`
- `reason=invalid_settings`
- `reason=authentication_failed`
- `reason=database_not_found`
- `reason=connection_failed`
- `reason=ssl_mismatch`
- `reason=timeout`
- `reason=open_failed`

典型问题对应：

- `driver_unavailable`
  - `QMYSQL` 驱动没加载成功
- `authentication_failed`
  - 用户名/密码错误
- `database_not_found`
  - 库名不存在
- `connection_failed`
  - IP、端口、网络、防火墙问题
- `ssl_mismatch`
  - 客户端 SSL 选项和服务端不匹配

### SQL 执行层日志

每次写库会先记录：

- `prepare | target=... | sql=... | binds=...`

成功时会记录：

- `success | rowsAffected=... | sql=... | binds=...`

失败时会尽量分类：

- `reason=duplicate_key`
- `reason=not_null_violation`
- `reason=foreign_key_violation`
- `reason=table_not_found`
- `reason=database_not_found`
- `reason=authentication_failed`
- `reason=driver_unavailable`
- `reason=connection_not_open`
- `reason=sql_syntax_error`
- `reason=timeout`
- `reason=sql_exec_failed`

重点说明：

- `duplicate_key`
  - 常见于唯一键冲突
- `not_null_violation`
  - 常见于字段不能为空，但业务数据传了空值
- `connection_not_open`
  - 写 SQL 前连接已经不可用

### 异步写线程日志

异步模式下，以下动作都会打日志：

- 入队：`Enqueue async ...`
- 开始写入：`Async store... started`
- 写入成功：`Async store... finished`
- 写入失败：`Async store... failed`

日志上下文会带上：

- `requestId`
- `board`
- `point`
- `line / program / device / operator`
- `insertedBoards / insertedPoints`
- `error`
- `forward`

这样可以直接从日志判断：

- 数据有没有真正进到异步线程
- 卡在连接、SQL 执行还是 JSON 明细文件
- 当前失败到底是数据库连不上、驱动没加载、字段为空还是重复键

补充：示例为了更容易验证“不良统计”，会自动生成多个不良点名称，不再长期只出现同一个不良点名。

## 3.6 MES 调试窗口

`HostSpcExampleWindow` 新增了 `Open MES Tester` 按钮，会打开一个图形化 MES 调试窗口。

这个窗口提供 3 个能力：

1. 启动一个本地 HTTP 接收器，模拟 MES 接口
2. 生成与当前项目结构一致的 `InspectionBatch`
3. 调用 `MesEventForwarder` 真正把批次发出去，并展示收到的 JSON 请求体

推荐联调流程：

1. 点击 `Open MES Tester`
2. 点击“启动接收器”
3. 点击“使用本地接收器地址”
4. 检查 `siteCode / stationCode / userName` 和示例板信息
5. 点击“发送示例批次”
6. 在右侧查看收到的请求体和发送日志

这样可以快速确认：

- 当前项目实际发给 MES 的字段长什么样
- 本地网络是否可通
- 超时、错误返回、路径写错时前端会看到什么

## 3.7 JSON 扩字段时怎么改

当前版本已经不推荐每加一个 JSON 字段就去改 `PointDetailInfo` 结构体成员。默认做法是：

1. 普通扩展字段
   - 直接写进 `PointRecordRow::detail.extraFields`
   - 适合模板版本、模板路径、ROI、算法参数、相机配置、调试痕迹等只需要进 JSON 的内容

2. 扩展字段显示名
   - 直接写进 `PointRecordRow::detail.fieldDisplayNames`
   - 用于控制详情 UI 中扩展字段的中文显示名称
   - 推荐写法是 `fieldDisplayNames["cameraProfile"] = "相机方案"` 这种“原始 key -> 显示名”映射

3. 算法规划字段
   - 直接写进 `PointRecordRow::detail.algorithmPlan`
   - 业务字段名固定为 `算法规划`
   - 类型固定为 `QJsonArray`
   - 每个数组元素建议是一个对象，至少包含节点名以及 `ok / ng` 分支

4. 固定摘要字段
   - 如果字段需要像“模板文件路径”“算法规划摘要”一样在详情弹窗顶部单独显示，再改：
   - `src/ui/dialogs/PointDetailDialog.cpp`
   - `src/ui/common/PointDetailFieldCatalog.h`

5. 只有在下面两种场景才建议改 `PointDetailInfo` 固定成员
   - 这个字段已经不是“扩展字段”，而是你要长期稳定支持的核心字段
   - 这个字段需要被代码逻辑频繁读写，不适合总是从 `extraFields` 里按 key 取值

建议规则：

- 要进数据库的字段：继续加在 `PointRecordRow` 和数据库表，不要塞进 `extraFields`
- 只进详情 JSON 的字段：优先放 `detail.extraFields`
- 只影响详情 UI 显示名的参数：放 `detail.fieldDisplayNames`
- 可变长算法流程：放 `detail.algorithmPlan`
- 只需要树形查看的字段：不用改 UI
- 需要一眼看到的关键字段：再补顶部摘要显示

## 4. 重要约束

- `InspectionBatch` 的语义是“整板替换点位”
- 同一 `boardCode` 多次提交 batch，最终结果以最后一次成功提交为准
- 空 batch 不会被 flush；没有点位时需要继续等待、补点或显式丢弃缓存
- `storePoint()` 按 `(boardCode, pointName)` 做幂等 upsert
- 如果开启 MES，写入完成后还会同步做一次 MES 转发

所以业务上建议：

- 优先 `storeBatch()`
- 点位流式上报时优先聚合再写
- 不要把多个线程直接共享同一个 `IngestService` 实例

## 5. 接入当前项目

这几个文件已经通过根 `CMakeLists.txt` 接入到仓库示例目标里。

如果你在自己的宿主工程里复用，最小接入方式是：

1. 复制以下 4 个文件到宿主项目：
   - `SpcWriteManager.h`
   - `SpcWriteManager.cpp`
   - `BoardBatchAggregator.h`
   - `BoardBatchAggregator.cpp`
2. 确保宿主目标能包含 `LaserSpc` 的 `src` 头文件
3. 确保宿主目标链接了 `LaserSpcCore`
4. 由于这几个类用了 `Q_OBJECT`，要让宿主工程开启 `AUTOMOC`

如果你也想复用仓库内这个示例窗口，再额外复制：

1. `HostSpcExampleWindow.h`
2. `HostSpcExampleWindow.cpp`
3. `main.cpp`
4. `examples/host_spc_writer/CMakeLists.txt` 里的目标组织方式

一个可直接落地的宿主工程 `CMakeLists.txt` 可以参考：

```cmake
cmake_minimum_required(VERSION 3.16)

project(HostClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql Charts Concurrent Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Sql Charts Concurrent Network)

add_subdirectory(third_party/LaserSpc)

add_library(HostSpcWriterSupport STATIC
    src/SpcWriteManager.cpp
    src/BoardBatchAggregator.cpp
)
target_include_directories(HostSpcWriterSupport PUBLIC src)
target_link_libraries(HostSpcWriterSupport PUBLIC LaserSpcCore)

add_executable(HostClient
    src/main.cpp
    src/HostSpcExampleWindow.cpp
)
target_link_libraries(HostClient PRIVATE
    HostSpcWriterSupport
    LaserSpcUi
    Qt${QT_VERSION_MAJOR}::Widgets
)
```

## 6. 后续可扩展点

如果后面你的生产并发量再上去，可以继续扩展成：

- 按产线分片 writer
- 按设备分片 writer
- 超时自动刷盘定时器
- 失败重试和死信队列
