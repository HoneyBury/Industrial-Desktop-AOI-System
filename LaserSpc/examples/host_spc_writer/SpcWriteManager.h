#pragma once

#include <QObject>
#include <QMutex>
#include <QThread>

#include "domain/Models.h"
#include "infrastructure/AppConfigService.h"

Q_DECLARE_METATYPE(LaserSpc::Domain::BoardRecordRow)
Q_DECLARE_METATYPE(LaserSpc::Domain::PointRecordRow)
Q_DECLARE_METATYPE(LaserSpc::Domain::InspectionBatch)
Q_DECLARE_METATYPE(LaserSpc::Domain::IngestResult)

namespace HostSpc {

class SpcWriteWorker final : public QObject {
    Q_OBJECT
public:
    explicit SpcWriteWorker(LaserSpc::Infrastructure::AppSettings settings, QObject* parent = nullptr);

public slots:
    void storeBatch(LaserSpc::Domain::InspectionBatch batch);
    void storeBoard(LaserSpc::Domain::BoardRecordRow row);
    void storePoint(LaserSpc::Domain::PointRecordRow row);

signals:
    void batchStored(QString requestId, LaserSpc::Domain::IngestResult result);
    void boardStored(QString boardCode, LaserSpc::Domain::IngestResult result);
    void pointStored(QString boardCode, QString pointName, LaserSpc::Domain::IngestResult result);

private:
    LaserSpc::Infrastructure::AppSettings m_settings;
};

class SpcWriteManager final : public QObject {
    Q_OBJECT
public:
    explicit SpcWriteManager(QObject* parent = nullptr);
    explicit SpcWriteManager(const LaserSpc::Infrastructure::AppSettings& settings, QObject* parent = nullptr);
    ~SpcWriteManager() override;

    void initializeFromConfig();
    void initialize(const LaserSpc::Infrastructure::AppSettings& settings);
    void initialize(const LaserSpc::Infrastructure::DatabaseSettings& database,
                    const LaserSpc::Infrastructure::MesSettings& mes = {});

    LaserSpc::Infrastructure::AppSettings settings() const;

    LaserSpc::Domain::IngestResult storeBatch(const LaserSpc::Domain::InspectionBatch& batch) const;
    LaserSpc::Domain::IngestResult storeBoard(const LaserSpc::Domain::BoardRecordRow& row) const;
    LaserSpc::Domain::IngestResult storePoint(const LaserSpc::Domain::PointRecordRow& row) const;

    void startAsyncWriter();
    void stopAsyncWriter();

    void enqueueBatch(LaserSpc::Domain::InspectionBatch batch);
    void enqueueBoard(LaserSpc::Domain::BoardRecordRow row);
    void enqueuePoint(LaserSpc::Domain::PointRecordRow row);

    static LaserSpc::Domain::InspectionBatch buildBatch(QString requestId,
                                                        LaserSpc::Domain::BoardRecordRow board,
                                                        QList<LaserSpc::Domain::PointRecordRow> points);

signals:
    void requestStoreBatch(LaserSpc::Domain::InspectionBatch batch);
    void requestStoreBoard(LaserSpc::Domain::BoardRecordRow row);
    void requestStorePoint(LaserSpc::Domain::PointRecordRow row);

    void batchStored(QString requestId, LaserSpc::Domain::IngestResult result);
    void boardStored(QString boardCode, LaserSpc::Domain::IngestResult result);
    void pointStored(QString boardCode, QString pointName, LaserSpc::Domain::IngestResult result);

private:
    LaserSpc::Infrastructure::AppSettings currentSettings() const;

    mutable QMutex m_mutex;
    LaserSpc::Infrastructure::AppSettings m_settings;
    QThread* m_workerThread = nullptr;
    SpcWriteWorker* m_worker = nullptr;
};

}  // namespace HostSpc
