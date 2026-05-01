#include "BoardBatchAggregator.h"

#include <QMutexLocker>
#include <QtGlobal>

#include <utility>

namespace HostSpc {

BoardBatchAggregator::BoardBatchAggregator(SpcWriteManager* writer, QObject* parent)
    : QObject(parent), m_writer(writer) {
    Q_ASSERT(m_writer);
    connect(m_writer, &SpcWriteManager::batchStored, this, &BoardBatchAggregator::handleBatchStored);
}

void BoardBatchAggregator::setFlushMode(FlushMode mode) {
    {
        QMutexLocker locker(&m_mutex);
        m_flushMode = mode;
    }

    if (mode == FlushMode::Async) {
        m_writer->startAsyncWriter();
    }
}

BoardBatchAggregator::FlushMode BoardBatchAggregator::flushMode() const {
    QMutexLocker locker(&m_mutex);
    return m_flushMode;
}

bool BoardBatchAggregator::upsertBoard(LaserSpc::Domain::BoardRecordRow board,
                                       QString requestId,
                                       int expectedPoints) {
    if (expectedPoints < -1) {
        return false;
    }

    const QString boardCode = board.boardCode.trimmed();
    if (boardCode.isEmpty()) {
        return false;
    }

    board.boardCode = boardCode;
    requestId = requestId.trimmed();

    ReadyBatch ready;
    bool shouldDispatch = false;

    {
        QMutexLocker locker(&m_mutex);
        PendingBoard& entry = m_pending[boardCode];
        if (!requestId.isEmpty()) {
            entry.requestId = requestId;
        }
        if (expectedPoints >= 0) {
            entry.expectedPoints = expectedPoints;
        }
        entry.board = std::move(board);
        entry.lastUpdate = nowUtc();

        shouldDispatch = tryTakeReadyBatchLocked(boardCode, &ready);
    }

    if (shouldDispatch) {
        dispatchReadyBatch(std::move(ready));
    }

    return true;
}

bool BoardBatchAggregator::appendPoint(QString boardCode, LaserSpc::Domain::PointRecordRow point) {
    boardCode = boardCode.trimmed();
    if (boardCode.isEmpty()) {
        boardCode = point.boardCode.trimmed();
    }
    if (boardCode.isEmpty()) {
        return false;
    }

    point.boardCode = boardCode;

    ReadyBatch ready;
    bool shouldDispatch = false;

    {
        QMutexLocker locker(&m_mutex);
        PendingBoard& entry = m_pending[boardCode];
        entry.points.append(std::move(point));
        entry.lastUpdate = nowUtc();

        shouldDispatch = tryTakeReadyBatchLocked(boardCode, &ready);
    }

    if (shouldDispatch) {
        dispatchReadyBatch(std::move(ready));
    }

    return true;
}

bool BoardBatchAggregator::setExpectedPoints(const QString& boardCode, int expectedPoints) {
    if (expectedPoints < 0) {
        return false;
    }

    const QString trimmedCode = boardCode.trimmed();
    if (trimmedCode.isEmpty()) {
        return false;
    }

    ReadyBatch ready;
    bool shouldDispatch = false;

    {
        QMutexLocker locker(&m_mutex);
        PendingBoard& entry = m_pending[trimmedCode];
        entry.expectedPoints = expectedPoints;
        entry.lastUpdate = nowUtc();

        shouldDispatch = tryTakeReadyBatchLocked(trimmedCode, &ready);
    }

    if (shouldDispatch) {
        dispatchReadyBatch(std::move(ready));
    }

    return true;
}

bool BoardBatchAggregator::markBoardComplete(const QString& boardCode) {
    const QString trimmedCode = boardCode.trimmed();
    if (trimmedCode.isEmpty()) {
        return false;
    }

    ReadyBatch ready;
    bool shouldDispatch = false;

    {
        QMutexLocker locker(&m_mutex);
        auto it = m_pending.find(trimmedCode);
        if (it == m_pending.end()) {
            return false;
        }

        it->complete = true;
        it->lastUpdate = nowUtc();

        shouldDispatch = tryTakeReadyBatchLocked(trimmedCode, &ready);
    }

    if (shouldDispatch) {
        dispatchReadyBatch(std::move(ready));
    }

    return true;
}

bool BoardBatchAggregator::flushBoard(const QString& boardCode) {
    const QString trimmedCode = boardCode.trimmed();
    if (trimmedCode.isEmpty()) {
        return false;
    }

    ReadyBatch ready;
    QString reason;
    bool shouldDispatch = false;

    {
        QMutexLocker locker(&m_mutex);
        shouldDispatch = tryTakeAnyBatchLocked(trimmedCode, &ready);
        if (!shouldDispatch) {
            const auto it = m_pending.find(trimmedCode);
            if (it == m_pending.end()) {
                reason = QObject::tr("Pending board not found.");
            } else {
                reason = QObject::tr("Board metadata missing, cannot build inspection batch.");
            }
        }
    }

    if (!reason.isEmpty()) {
        emit batchDeferred(trimmedCode, reason);
    }

    if (shouldDispatch) {
        dispatchReadyBatch(std::move(ready));
    }

    return shouldDispatch;
}

int BoardBatchAggregator::flushReadyBoards() {
    QList<ReadyBatch> readyBatches;

    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_pending.begin(); it != m_pending.end();) {
            if (!it->board.has_value() || !isReadyToFlush(it.value())) {
                ++it;
                continue;
            }

            const QString boardCode = it.key();
            readyBatches.append(buildReadyBatch(boardCode, std::move(it.value())));
            it = m_pending.erase(it);
        }
    }

    for (auto& ready : readyBatches) {
        dispatchReadyBatch(std::move(ready));
    }

    return readyBatches.size();
}

int BoardBatchAggregator::flushExpired(qint64 maxPendingMs, bool requireComplete) {
    if (maxPendingMs < 0) {
        return 0;
    }

    QList<ReadyBatch> readyBatches;
    QList<QString> deferredBoards;
    const QDateTime deadline = nowUtc().addMSecs(-maxPendingMs);

    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_pending.begin(); it != m_pending.end();) {
            if (!it->board.has_value()) {
                ++it;
                continue;
            }

            const bool expired = !it->lastUpdate.isValid() || it->lastUpdate <= deadline;
            const bool eligible = expired && (!requireComplete || it->complete);
            if (!eligible) {
                ++it;
                continue;
            }

            if (!hasFlushablePoints(it.value())) {
                deferredBoards.append(it.key());
                ++it;
                continue;
            }

            const QString boardCode = it.key();
            readyBatches.append(buildReadyBatch(boardCode, std::move(it.value())));
            it = m_pending.erase(it);
        }
    }

    emitDeferredForNonFlushableBoards(deferredBoards,
                                      QObject::tr("Point records are empty, refusing to flush an empty inspection batch."));

    for (auto& ready : readyBatches) {
        dispatchReadyBatch(std::move(ready));
    }

    return readyBatches.size();
}

int BoardBatchAggregator::flushAll() {
    QList<ReadyBatch> readyBatches;
    QList<QString> deferredBoards;

    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_pending.begin(); it != m_pending.end();) {
            if (!it->board.has_value()) {
                ++it;
                continue;
            }

            if (!hasFlushablePoints(it.value())) {
                deferredBoards.append(it.key());
                ++it;
                continue;
            }

            const QString boardCode = it.key();
            readyBatches.append(buildReadyBatch(boardCode, std::move(it.value())));
            it = m_pending.erase(it);
        }
    }

    emitDeferredForNonFlushableBoards(deferredBoards,
                                      QObject::tr("Point records are empty, refusing to flush an empty inspection batch."));

    for (auto& ready : readyBatches) {
        dispatchReadyBatch(std::move(ready));
    }

    return readyBatches.size();
}

bool BoardBatchAggregator::removeBoard(const QString& boardCode) {
    const QString trimmedCode = boardCode.trimmed();
    if (trimmedCode.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    const bool removed = m_pending.remove(trimmedCode);
    return removed;
}

int BoardBatchAggregator::pendingBoardCount() const {
    QMutexLocker locker(&m_mutex);
    return m_pending.size();
}

int BoardBatchAggregator::pendingPointCount(const QString& boardCode) const {
    const QString trimmedCode = boardCode.trimmed();
    if (trimmedCode.isEmpty()) {
        return 0;
    }

    QMutexLocker locker(&m_mutex);
    const auto it = m_pending.constFind(trimmedCode);
    if (it == m_pending.cend()) {
        return 0;
    }

    return it->points.size();
}

void BoardBatchAggregator::handleBatchStored(QString requestId, LaserSpc::Domain::IngestResult result) {
    InflightBatch inflight;
    bool found = false;

    {
        QMutexLocker locker(&m_mutex);
        auto it = m_inflightBatchesByRequestId.find(requestId);
        if (it != m_inflightBatchesByRequestId.end() && !it->isEmpty()) {
            inflight = it->dequeue();
            if (it->isEmpty()) {
                m_inflightBatchesByRequestId.erase(it);
            }
            found = true;
        }
    }

    if (!found) {
        return;
    }

    emit batchFlushed(inflight.boardCode, inflight.requestId, result);
}

bool BoardBatchAggregator::isReadyToFlush(const PendingBoard& entry) {
    if (!entry.board.has_value()) {
        return false;
    }

    if (entry.complete) {
        return hasFlushablePoints(entry);
    }

    return entry.expectedPoints >= 0 && entry.points.size() >= entry.expectedPoints && hasFlushablePoints(entry);
}

bool BoardBatchAggregator::hasFlushablePoints(const PendingBoard& entry) {
    return !entry.points.isEmpty();
}

BoardBatchAggregator::ReadyBatch BoardBatchAggregator::buildReadyBatch(const QString& boardCode, PendingBoard entry) {
    ReadyBatch ready;
    ready.boardCode = boardCode;
    ready.batch = SpcWriteManager::buildBatch(entry.requestId,
                                              std::move(entry.board.value()),
                                              std::move(entry.points));
    return ready;
}

QDateTime BoardBatchAggregator::nowUtc() {
    return QDateTime::currentDateTimeUtc();
}

void BoardBatchAggregator::dispatchReadyBatch(ReadyBatch ready) {
    emit batchPrepared(ready.boardCode, ready.batch.requestId, ready.batch.points.size());

    if (flushMode() == FlushMode::Async) {
        {
            QMutexLocker locker(&m_mutex);
            m_inflightBatchesByRequestId[ready.batch.requestId].enqueue(
                InflightBatch{ready.boardCode, ready.batch.requestId});
        }

        m_writer->enqueueBatch(std::move(ready.batch));
        return;
    }

    const auto result = m_writer->storeBatch(ready.batch);
    emit batchFlushed(ready.boardCode, ready.batch.requestId, result);
}

void BoardBatchAggregator::emitDeferredForNonFlushableBoards(const QList<QString>& boardCodes, const QString& reason) {
    for (const QString& boardCode : boardCodes) {
        emit batchDeferred(boardCode, reason);
    }
}

bool BoardBatchAggregator::tryTakeReadyBatchLocked(const QString& boardCode, ReadyBatch* ready) {
    auto it = m_pending.find(boardCode);
    if (it == m_pending.end()) {
        return false;
    }

    if (!it->board.has_value() || !isReadyToFlush(it.value())) {
        return false;
    }

    *ready = buildReadyBatch(boardCode, std::move(it.value()));
    m_pending.erase(it);
    return true;
}

bool BoardBatchAggregator::tryTakeAnyBatchLocked(const QString& boardCode, ReadyBatch* ready) {
    auto it = m_pending.find(boardCode);
    if (it == m_pending.end()) {
        return false;
    }

    if (!it->board.has_value() || !hasFlushablePoints(it.value())) {
        return false;
    }

    *ready = buildReadyBatch(boardCode, std::move(it.value()));
    m_pending.erase(it);
    return true;
}

}  // namespace HostSpc
