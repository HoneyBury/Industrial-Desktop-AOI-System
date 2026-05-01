#include "SpcWriteManager.h"

#include <QDateTime>
#include <QMetaType>
#include <QMutexLocker>
#include <QUuid>

#include <atomic>
#include <memory>
#include <utility>

#include "app/IngestService.h"
#include "infrastructure/AppConfigService.h"
#include "infrastructure/Logger.h"
#include "infrastructure/PointDetailJsonService.h"
#include "infrastructure/MesEventForwarder.h"
#include "infrastructure/MySqlSpcWriteRepository.h"

namespace HostSpc {

namespace {

QString makeDefaultRequestId() {
    static std::atomic<quint64> sequence{1};
    const quint64 suffix = sequence.fetch_add(1, std::memory_order_relaxed);
    return QObject::tr("batch-%1-%2-%3")
        .arg(QDateTime::currentDateTime().toString(QObject::tr("yyyyMMddHHmmsszzz")))
        .arg(suffix)
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

LaserSpc::Domain::PointDetailInfo buildPointDetailInfo(const LaserSpc::Domain::PointRecordRow& row) {
    LaserSpc::Domain::PointDetailInfo detail = row.detail;
    if (detail.laserTemplatePath.trimmed().isEmpty()) {
        const QString configuredPath = detail.extraFields.value(QStringLiteral("templateFilePath")).toString().trimmed();
        detail.laserTemplatePath = configuredPath.isEmpty() ? QObject::tr("templates/%1.tpl").arg(row.programName)
                                                            : configuredPath;
    }
    if (detail.laserContent.trimmed().isEmpty()) {
        detail.laserContent = row.laserContent.trimmed().isEmpty() ? QObject::tr("%1-LASER").arg(row.pointName) : row.laserContent;
    }
    if (detail.readCodeContent.trimmed().isEmpty()) {
        detail.readCodeContent = row.readCodeContent;
    }
    if (detail.programName.trimmed().isEmpty()) {
        detail.programName = row.programName;
    }
    if (!detail.startTime.isValid()) {
        detail.startTime = row.startTime;
    }
    if (!detail.endTime.isValid()) {
        detail.endTime = row.endTime;
    }
    detail.success = (row.result.trimmed().toUpper() == QObject::tr("OK"));
    return detail;
}

bool ensurePointDetailFile(LaserSpc::Domain::PointRecordRow* row, QString* warningMessage = nullptr) {
    if (row == nullptr) {
        if (warningMessage != nullptr) {
            *warningMessage = QObject::tr("Point record must not be null.");
        }
        return false;
    }

    if (row->detailJsonPath.trimmed().isEmpty()) {
        row->detailJsonPath = LaserSpc::Infrastructure::PointDetailJsonService::buildDefaultFilePath(*row);
    }
    if (row->laserContent.trimmed().isEmpty()) {
        row->laserContent = QObject::tr("%1-LASER").arg(row->pointName);
    }

    const auto detail = buildPointDetailInfo(*row);
    QString saveError;
    if (!LaserSpc::Infrastructure::PointDetailJsonService::saveDetail(detail, row->detailJsonPath, &saveError)) {
        row->detailJsonPath.clear();
        if (warningMessage != nullptr) {
            *warningMessage = saveError;
        }
        return false;
    }

    if (warningMessage != nullptr) {
        warningMessage->clear();
    }
    return true;
}

QString joinWarnings(const QStringList& warnings) {
    if (warnings.isEmpty()) {
        return {};
    }
    return QObject::tr("Detail JSON warnings: %1").arg(warnings.join(QObject::tr(" ; ")));
}

void appendForwardMessage(LaserSpc::Domain::IngestResult* result, const QString& message) {
    if (result == nullptr || message.trimmed().isEmpty()) {
        return;
    }
    if (result->forwardMessage.trimmed().isEmpty()) {
        result->forwardMessage = message;
        return;
    }
    result->forwardMessage += QObject::tr(" | ") + message;
}

void ensureBatchDetailFiles(LaserSpc::Domain::InspectionBatch* batch, QStringList* warnings) {
    if (batch == nullptr) {
        return;
    }

    for (int index = 0; index < batch->points.size(); ++index) {
        QString warning;
        ensurePointDetailFile(&batch->points[index], &warning);
        if (warnings != nullptr && !warning.trimmed().isEmpty()) {
            warnings->append(
                QObject::tr("point=%1 error=%2")
                    .arg(batch->points[index].pointName, warning));
        }
    }
}

void applyBatchDefaults(LaserSpc::Domain::InspectionBatch* batch) {
    if (!batch) {
        return;
    }

    if (batch->requestId.trimmed().isEmpty()) {
        batch->requestId = makeDefaultRequestId();
    }

    if (!batch->board.eventTime.isValid()) {
        batch->board.eventTime = QDateTime::currentDateTime();
    } else {
        batch->board.eventTime = batch->board.eventTime.toLocalTime();
    }

    for (auto& point : batch->points) {
        if (point.boardCode.trimmed().isEmpty()) {
            point.boardCode = batch->board.boardCode;
        }
        if (point.lineName.trimmed().isEmpty()) {
            point.lineName = batch->board.lineName;
        }
        if (point.programName.trimmed().isEmpty()) {
            point.programName = batch->board.programName;
        }
        if (point.deviceName.trimmed().isEmpty()) {
            point.deviceName = batch->board.deviceName;
        }
        if (!point.endTime.isValid()) {
            point.endTime = batch->board.eventTime;
        } else {
            point.endTime = point.endTime.toLocalTime();
        }
        if (!point.startTime.isValid()) {
            point.startTime = point.endTime;
        } else {
            point.startTime = point.startTime.toLocalTime();
        }
    }
}

LaserSpc::App::IngestService buildIngestService(const LaserSpc::Infrastructure::AppSettings& settings) {
    auto repository = std::make_shared<LaserSpc::Infrastructure::MySqlSpcWriteRepository>(settings.database);

    LaserSpc::App::IngestService service(repository);

    LaserSpc::Infrastructure::MesEventForwarder mesForwarder(settings.mes);
    service.setMesForwarder(
        [mesForwarder](const LaserSpc::Domain::InspectionBatch& batch, QString* errorMessage) mutable {
            return mesForwarder.forwardInspectionBatch(batch, errorMessage);
        });

    return service;
}

LaserSpc::Domain::IngestResult doStoreBatch(const LaserSpc::Infrastructure::AppSettings& settings,
                                            LaserSpc::Domain::InspectionBatch batch) {
    applyBatchDefaults(&batch);
    QStringList detailWarnings;
    ensureBatchDetailFiles(&batch, &detailWarnings);
    auto service = buildIngestService(settings);
    auto result = service.ingestInspectionBatch(std::move(batch));
    appendForwardMessage(&result, joinWarnings(detailWarnings));
    return result;
}

LaserSpc::Domain::IngestResult doStoreBoard(const LaserSpc::Infrastructure::AppSettings& settings,
                                            LaserSpc::Domain::BoardRecordRow row) {
    auto service = buildIngestService(settings);
    return service.ingestBoardRecord(std::move(row));
}

LaserSpc::Domain::IngestResult doStorePoint(const LaserSpc::Infrastructure::AppSettings& settings,
                                            LaserSpc::Domain::PointRecordRow row) {
    QString detailWarning;
    ensurePointDetailFile(&row, &detailWarning);
    auto service = buildIngestService(settings);
    auto result = service.ingestPointRecord(std::move(row));
    appendForwardMessage(&result, detailWarning.trimmed().isEmpty()
                                      ? QString()
                                      : QObject::tr("Detail JSON warning: %1").arg(detailWarning));
    return result;
}

}  // namespace

SpcWriteWorker::SpcWriteWorker(LaserSpc::Infrastructure::AppSettings settings, QObject* parent)
    : QObject(parent), m_settings(std::move(settings)) {}

void SpcWriteWorker::storeBatch(LaserSpc::Domain::InspectionBatch batch) {
    applyBatchDefaults(&batch);
    const QString requestId = batch.requestId;
    LaserSpc::Infrastructure::Logger::info(
        QObject::tr("Async storeBatch started | requestId=%1 board=%2 points=%3 line=%4 program=%5 device=%6")
            .arg(requestId, batch.board.boardCode)
            .arg(batch.points.size())
            .arg(batch.board.lineName, batch.board.programName, batch.board.deviceName));
    const auto result = doStoreBatch(m_settings, std::move(batch));
    if (result.success) {
        LaserSpc::Infrastructure::Logger::info(
            QObject::tr("Async storeBatch finished | requestId=%1 success=1 insertedBoards=%2 insertedPoints=%3 error=%4 forward=%5")
                .arg(requestId)
                .arg(result.insertedBoards)
                .arg(result.insertedPoints)
                .arg(result.errorMessage, result.forwardMessage));
    } else {
        LaserSpc::Infrastructure::Logger::error(
            QObject::tr("Async storeBatch failed | requestId=%1 success=0 error=%2 insertedBoards=%3 insertedPoints=%4 forward=%5")
                .arg(requestId, result.errorMessage, result.forwardMessage));
    }
    emit batchStored(requestId, result);
}

void SpcWriteWorker::storeBoard(LaserSpc::Domain::BoardRecordRow row) {
    const QString boardCode = row.boardCode;
    LaserSpc::Infrastructure::Logger::info(
        QObject::tr("Async storeBoard started | board=%1 result=%2 line=%3 program=%4 device=%5 operator=%6")
            .arg(boardCode, row.result, row.lineName, row.programName, row.deviceName, row.operatorName));
    const auto result = doStoreBoard(m_settings, std::move(row));
    if (result.success) {
        LaserSpc::Infrastructure::Logger::info(
            QObject::tr("Async storeBoard finished | board=%1 success=1 insertedBoards=%2 error=%3 forward=%4")
                .arg(boardCode)
                .arg(result.insertedBoards)
                .arg(result.errorMessage, result.forwardMessage));
    } else {
        LaserSpc::Infrastructure::Logger::error(
            QObject::tr("Async storeBoard failed | board=%1 success=0 error=%2 insertedBoards=%3 forward=%4")
                .arg(boardCode)
                .arg(result.errorMessage)
                .arg(result.insertedBoards)
                .arg(result.forwardMessage));
    }
    emit boardStored(boardCode, result);
}

void SpcWriteWorker::storePoint(LaserSpc::Domain::PointRecordRow row) {
    const QString boardCode = row.boardCode;
    const QString pointName = row.pointName;
    LaserSpc::Infrastructure::Logger::info(
        QObject::tr("Async storePoint started | board=%1 point=%2 result=%3 grade=%4 device=%5 detailPath=%6")
            .arg(boardCode, pointName, row.result, row.readGrade, row.deviceName, row.detailJsonPath));
    const auto result = doStorePoint(m_settings, std::move(row));
    if (result.success) {
        LaserSpc::Infrastructure::Logger::info(
            QObject::tr("Async storePoint finished | board=%1 point=%2 success=1 insertedPoints=%3 error=%4 forward=%5")
                .arg(boardCode, pointName)
                .arg(result.insertedPoints)
                .arg(result.errorMessage, result.forwardMessage));
    } else {
        LaserSpc::Infrastructure::Logger::error(
            QObject::tr("Async storePoint failed | board=%1 point=%2 success=0 error=%3 insertedPoints=%4 forward=%5")
                .arg(boardCode, pointName)
                .arg(result.errorMessage)
                .arg(result.insertedPoints)
                .arg(result.forwardMessage));
    }
    emit pointStored(boardCode, pointName, result);
}

SpcWriteManager::SpcWriteManager(QObject* parent) : QObject(parent) {
    initializeFromConfig();
}

SpcWriteManager::SpcWriteManager(const LaserSpc::Infrastructure::AppSettings& settings, QObject* parent)
    : QObject(parent), m_settings(settings) {}

SpcWriteManager::~SpcWriteManager() {
    stopAsyncWriter();
}

void SpcWriteManager::initializeFromConfig() {
    LaserSpc::Infrastructure::AppConfigService configService;
    initialize(configService.settings());
}

void SpcWriteManager::initialize(const LaserSpc::Infrastructure::AppSettings& settings) {
    bool restartWriter = false;
    {
        QMutexLocker locker(&m_mutex);
        restartWriter = (m_workerThread != nullptr);
        m_settings = settings;
    }

    if (restartWriter) {
        stopAsyncWriter();
        startAsyncWriter();
    }
}

void SpcWriteManager::initialize(const LaserSpc::Infrastructure::DatabaseSettings& database,
                                 const LaserSpc::Infrastructure::MesSettings& mes) {
    LaserSpc::Infrastructure::AppSettings settings;
    settings.useMySql = true;
    settings.database = database;
    settings.mes = mes;
    initialize(settings);
}

LaserSpc::Infrastructure::AppSettings SpcWriteManager::settings() const {
    return currentSettings();
}

LaserSpc::Infrastructure::AppSettings SpcWriteManager::currentSettings() const {
    QMutexLocker locker(&m_mutex);
    return m_settings;
}

LaserSpc::Domain::IngestResult SpcWriteManager::storeBatch(const LaserSpc::Domain::InspectionBatch& batch) const {
    return doStoreBatch(currentSettings(), batch);
}

LaserSpc::Domain::IngestResult SpcWriteManager::storeBoard(const LaserSpc::Domain::BoardRecordRow& row) const {
    return doStoreBoard(currentSettings(), row);
}

LaserSpc::Domain::IngestResult SpcWriteManager::storePoint(const LaserSpc::Domain::PointRecordRow& row) const {
    return doStorePoint(currentSettings(), row);
}

void SpcWriteManager::startAsyncWriter() {
    QMutexLocker locker(&m_mutex);
    if (m_workerThread) {
        return;
    }

    qRegisterMetaType<LaserSpc::Domain::BoardRecordRow>("LaserSpc::Domain::BoardRecordRow");
    qRegisterMetaType<LaserSpc::Domain::PointRecordRow>("LaserSpc::Domain::PointRecordRow");
    qRegisterMetaType<LaserSpc::Domain::InspectionBatch>("LaserSpc::Domain::InspectionBatch");
    qRegisterMetaType<LaserSpc::Domain::IngestResult>("LaserSpc::Domain::IngestResult");

    m_workerThread = new QThread(this);
    m_worker = new SpcWriteWorker(m_settings);
    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::started, this, []() {
        LaserSpc::Infrastructure::Logger::info("Async writer thread started.");
    });
    connect(m_workerThread, &QThread::finished, this, []() {
        LaserSpc::Infrastructure::Logger::info("Async writer thread finished.");
    });

    connect(this,
            &SpcWriteManager::requestStoreBatch,
            m_worker,
            &SpcWriteWorker::storeBatch,
            Qt::QueuedConnection);
    connect(this,
            &SpcWriteManager::requestStoreBoard,
            m_worker,
            &SpcWriteWorker::storeBoard,
            Qt::QueuedConnection);
    connect(this,
            &SpcWriteManager::requestStorePoint,
            m_worker,
            &SpcWriteWorker::storePoint,
            Qt::QueuedConnection);

    connect(m_worker, &SpcWriteWorker::batchStored, this, &SpcWriteManager::batchStored);
    connect(m_worker, &SpcWriteWorker::boardStored, this, &SpcWriteManager::boardStored);
    connect(m_worker, &SpcWriteWorker::pointStored, this, &SpcWriteManager::pointStored);

    m_workerThread->start();
}

void SpcWriteManager::stopAsyncWriter() {
    QThread* thread = nullptr;
    SpcWriteWorker* worker = nullptr;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_workerThread) {
            return;
        }

        thread = m_workerThread;
        worker = m_worker;
        m_workerThread = nullptr;
        m_worker = nullptr;
    }

    disconnect(this, nullptr, worker, nullptr);
    disconnect(worker, nullptr, this, nullptr);

    thread->quit();
    thread->wait();

    delete worker;
    delete thread;
}

void SpcWriteManager::enqueueBatch(LaserSpc::Domain::InspectionBatch batch) {
    startAsyncWriter();
    LaserSpc::Infrastructure::Logger::info(
        QObject::tr("Enqueue async batch | requestId=%1 board=%2 points=%3 line=%4 program=%5 device=%6")
            .arg(batch.requestId, batch.board.boardCode)
            .arg(batch.points.size())
            .arg(batch.board.lineName, batch.board.programName, batch.board.deviceName));
    emit requestStoreBatch(std::move(batch));
}

void SpcWriteManager::enqueueBoard(LaserSpc::Domain::BoardRecordRow row) {
    startAsyncWriter();
    LaserSpc::Infrastructure::Logger::info(
        QObject::tr("Enqueue async board | board=%1 result=%2 line=%3 program=%4 device=%5 operator=%6")
            .arg(row.boardCode, row.result, row.lineName, row.programName, row.deviceName, row.operatorName));
    emit requestStoreBoard(std::move(row));
}

void SpcWriteManager::enqueuePoint(LaserSpc::Domain::PointRecordRow row) {
    startAsyncWriter();
    LaserSpc::Infrastructure::Logger::info(
        QObject::tr("Enqueue async point | board=%1 point=%2 result=%3 grade=%4 device=%5 detailPath=%6")
            .arg(row.boardCode, row.pointName, row.result, row.readGrade, row.deviceName, row.detailJsonPath));
    emit requestStorePoint(std::move(row));
}

LaserSpc::Domain::InspectionBatch SpcWriteManager::buildBatch(QString requestId,
                                                              LaserSpc::Domain::BoardRecordRow board,
                                                              QList<LaserSpc::Domain::PointRecordRow> points) {
    LaserSpc::Domain::InspectionBatch batch;
    batch.requestId = std::move(requestId);
    batch.board = std::move(board);
    batch.points = std::move(points);
    applyBatchDefaults(&batch);
    return batch;
}

}  // namespace HostSpc
