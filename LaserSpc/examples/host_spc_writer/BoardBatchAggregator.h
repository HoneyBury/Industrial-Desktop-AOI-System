#pragma once

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QQueue>

#include <optional>

#include "SpcWriteManager.h"

namespace HostSpc {

class BoardBatchAggregator final : public QObject {
    Q_OBJECT
public:
    enum class FlushMode {
        Sync,
        Async
    };

    explicit BoardBatchAggregator(SpcWriteManager* writer, QObject* parent = nullptr);
    ~BoardBatchAggregator() override = default;

    void setFlushMode(FlushMode mode);
    FlushMode flushMode() const;

    bool upsertBoard(LaserSpc::Domain::BoardRecordRow board,
                     QString requestId = QString(),
                     int expectedPoints = -1);

    bool appendPoint(QString boardCode, LaserSpc::Domain::PointRecordRow point);

    bool setExpectedPoints(const QString& boardCode, int expectedPoints);
    bool markBoardComplete(const QString& boardCode);

    bool flushBoard(const QString& boardCode);
    int flushReadyBoards();
    int flushExpired(qint64 maxPendingMs, bool requireComplete = false);
    int flushAll();

    bool removeBoard(const QString& boardCode);

    int pendingBoardCount() const;
    int pendingPointCount(const QString& boardCode) const;

signals:
    void batchPrepared(QString boardCode, QString requestId, int pointCount);
    void batchFlushed(QString boardCode, QString requestId, LaserSpc::Domain::IngestResult result);
    void batchDeferred(QString boardCode, QString reason);

private slots:
    void handleBatchStored(QString requestId, LaserSpc::Domain::IngestResult result);

private:
    struct PendingBoard {
        QString requestId;
        std::optional<LaserSpc::Domain::BoardRecordRow> board;
        QList<LaserSpc::Domain::PointRecordRow> points;
        int expectedPoints = -1;
        bool complete = false;
        QDateTime lastUpdate = QDateTime::currentDateTimeUtc();
    };

    struct InflightBatch {
        QString boardCode;
        QString requestId;
    };

    struct ReadyBatch {
        QString boardCode;
        LaserSpc::Domain::InspectionBatch batch;
    };

    static bool isReadyToFlush(const PendingBoard& entry);
    static bool hasFlushablePoints(const PendingBoard& entry);
    static ReadyBatch buildReadyBatch(const QString& boardCode, PendingBoard entry);
    static QDateTime nowUtc();

    void dispatchReadyBatch(ReadyBatch ready);
    void emitDeferredForNonFlushableBoards(const QList<QString>& boardCodes, const QString& reason);
    bool tryTakeReadyBatchLocked(const QString& boardCode, ReadyBatch* ready);
    bool tryTakeAnyBatchLocked(const QString& boardCode, ReadyBatch* ready);

    SpcWriteManager* m_writer = nullptr;

    mutable QMutex m_mutex;
    QHash<QString, PendingBoard> m_pending;
    QHash<QString, QQueue<InflightBatch>> m_inflightBatchesByRequestId;
    FlushMode m_flushMode = FlushMode::Sync;
};

}  // namespace HostSpc
