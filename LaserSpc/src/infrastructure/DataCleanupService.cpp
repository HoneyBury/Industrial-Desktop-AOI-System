#include "infrastructure/DataCleanupService.h"

#include <functional>

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QStringList>
#include <QVariant>

#include "infrastructure/Logger.h"
#include "infrastructure/MySqlTransactionExecutor.h"

namespace LaserSpc::Infrastructure {

namespace {

constexpr int kCleanupBatchSize = 200;
constexpr int kCleanupRetryCount = 3;

QStringList seedBoardCodes() {
    return {
        "BD-240301-0001",
        "BD-240301-0002",
        "BD-240301-0003",
        "BD-240301-0004",
        "BD-240301-0005",
        "BD-240301-0006",
        "BD-240301-0007",
        "BD-240301-0008",
        "BD-240301-0009",
        "BD-240301-0010"
    };
}

QString placeholderList(int count) {
    QStringList values;
    for (int index = 0; index < count; ++index) {
        values.append("?");
    }
    return values.join(", ");
}

bool isRetryableCleanupError(const QString& errorMessage) {
    const QString normalized = errorMessage.trimmed().toLower();
    return normalized.contains("deadlock") || normalized.contains("lock wait timeout") ||
           normalized.contains("try restarting transaction");
}

bool execQuery(QSqlDatabase& database,
               const QString& sql,
               const QVariantList& binds,
               QSqlQuery* executedQuery,
               QString* errorMessage) {
    QSqlQuery query(database);
    query.prepare(sql);
    for (const QVariant& value : binds) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    if (executedQuery != nullptr) {
        *executedQuery = std::move(query);
    }
    return true;
}

bool fetchBoardCodes(QSqlDatabase& database,
                     const QString& sql,
                     const QVariantList& binds,
                     QStringList* boardCodes,
                     QString* errorMessage) {
    QSqlQuery query;
    if (!execQuery(database, sql, binds, &query, errorMessage)) {
        return false;
    }

    QStringList values;
    while (query.next()) {
        values.append(query.value(0).toString());
    }

    if (boardCodes != nullptr) {
        *boardCodes = values;
    }
    return true;
}

bool fetchScalarInt(QSqlDatabase& database,
                    const QString& sql,
                    const QVariantList& binds,
                    int* value,
                    QString* errorMessage) {
    QSqlQuery query;
    if (!execQuery(database, sql, binds, &query, errorMessage)) {
        return false;
    }

    if (!query.next()) {
        if (value != nullptr) {
            *value = 0;
        }
        return true;
    }

    if (value != nullptr) {
        *value = query.value(0).toInt();
    }
    return true;
}

bool execDelete(QSqlDatabase& database,
                const QString& sql,
                const QVariantList& binds,
                int* deletedRows,
                QString* errorMessage) {
    QSqlQuery query;
    if (!execQuery(database, sql, binds, &query, errorMessage)) {
        return false;
    }

    if (deletedRows != nullptr) {
        *deletedRows = query.numRowsAffected();
    }
    return true;
}

DataCleanupResult executeCleanupTransaction(const DatabaseSettings& settings,
                                           const QString& scope,
                                           const std::function<bool(QSqlDatabase&, DataCleanupResult*)>& body) {
    DataCleanupResult lastResult;

    for (int attempt = 1; attempt <= kCleanupRetryCount; ++attempt) {
        DataCleanupResult result;
        result.retryCount = attempt - 1;

        auto lease = DatabaseConnection::acquireMySql(settings);
        if (!lease.isOpen()) {
            result.errorMessage = lease.lastError();
            return result;
        }

        QSqlDatabase database = lease.database();
        if (!MySqlTransactionExecutor::begin(database, scope, &result.errorMessage)) {
            return result;
        }

        if (!body(database, &result)) {
            MySqlTransactionExecutor::rollback(database, scope);
            lastResult = result;
        } else if (!MySqlTransactionExecutor::commit(database, scope, &result.errorMessage)) {
            MySqlTransactionExecutor::rollback(database, scope);
            lastResult = result;
        } else {
            result.success = true;
            result.retryCount = attempt - 1;
            return result;
        }

        lastResult.retryCount = attempt;
        if (attempt >= kCleanupRetryCount || !isRetryableCleanupError(lastResult.errorMessage)) {
            return lastResult;
        }

        Logger::warn(QString("%1 retrying after transient cleanup failure (attempt %2/%3): %4")
                         .arg(scope)
                         .arg(attempt)
                         .arg(kCleanupRetryCount)
                         .arg(lastResult.errorMessage));
        QThread::msleep(static_cast<unsigned long>(50 * attempt));
    }

    return lastResult;
}

void mergeCleanupResult(const DataCleanupResult& step, DataCleanupResult* total) {
    if (total == nullptr) {
        return;
    }

    total->deletedPointRecords += step.deletedPointRecords;
    total->deletedBoardRecords += step.deletedBoardRecords;
    total->processedBatches += step.processedBatches;
    total->retryCount += step.retryCount;
    if (step.cutoffTime.isValid()) {
        total->cutoffTime = step.cutoffTime;
    }
}

bool deleteBoardsByCodes(QSqlDatabase& database,
                         const QStringList& boardCodes,
                         DataCleanupResult* result,
                         QString* errorMessage) {
    if (boardCodes.isEmpty()) {
        return true;
    }

    QVariantList binds;
    for (const QString& boardCode : boardCodes) {
        binds.append(boardCode);
    }

    int pointCount = 0;
    if (!fetchScalarInt(database,
                        QString("SELECT COUNT(*) FROM point_records WHERE board_code IN (%1)")
                            .arg(placeholderList(boardCodes.size())),
                        binds,
                        &pointCount,
                        errorMessage)) {
        return false;
    }

    int boardCount = 0;
    if (!execDelete(database,
                    QString("DELETE FROM board_records WHERE board_code IN (%1)")
                        .arg(placeholderList(boardCodes.size())),
                    binds,
                    &boardCount,
                    errorMessage)) {
        return false;
    }

    if (result != nullptr) {
        result->deletedPointRecords += pointCount;
        result->deletedBoardRecords += boardCount;
        result->processedBatches += 1;
    }
    return true;
}

DataCleanupResult deleteSeedBoardsBatch(const DatabaseSettings& settings, const QStringList& boardCodes) {
    return executeCleanupTransaction(
        settings,
        QObject::tr("DataCleanup.seed.boards"),
        [boardCodes](QSqlDatabase& database, DataCleanupResult* result) {
            return deleteBoardsByCodes(database, boardCodes, result, &result->errorMessage);
        });
}

DataCleanupResult deleteOrphanSeedPointsBatch(const DatabaseSettings& settings, const QStringList& seedCodes) {
    return executeCleanupTransaction(
        settings,
        QObject::tr("DataCleanup.seed.orphans"),
        [seedCodes](QSqlDatabase& database, DataCleanupResult* result) {
            QVariantList binds;
            for (const QString& boardCode : seedCodes) {
                binds.append(boardCode);
            }

            int deleted = 0;
            if (!execDelete(
                    database,
                    QString("DELETE FROM point_records "
                            "WHERE (detail_json_path LIKE 'seed_data/point_details/%%' OR board_code IN (%1)) "
                            "AND NOT EXISTS (SELECT 1 FROM board_records WHERE board_records.board_code = point_records.board_code)")
                        .arg(placeholderList(seedCodes.size())),
                    binds,
                    &deleted,
                    &result->errorMessage)) {
                return false;
            }

            result->deletedPointRecords += deleted;
            if (deleted > 0) {
                result->processedBatches += 1;
            }
            return true;
        });
}

DataCleanupResult fetchDatabaseCurrentTimeResult(const DatabaseSettings& settings) {
    return executeCleanupTransaction(
        settings,
        QObject::tr("DataCleanup.production.clock"),
        [](QSqlDatabase& database, DataCleanupResult* result) {
            QSqlQuery query;
            if (!execQuery(database, "SELECT CURRENT_TIMESTAMP()", {}, &query, &result->errorMessage)) {
                return false;
            }
            if (!query.next()) {
                result->errorMessage = QObject::tr("Failed to read database current timestamp.");
                return false;
            }

            result->cutoffTime = query.value(0).toDateTime();
            return true;
        });
}

DataCleanupResult deleteEligibleProductionBoardsBatch(const DatabaseSettings& settings, const QDateTime& cutoffTime) {
    return executeCleanupTransaction(
        settings,
        QObject::tr("DataCleanup.production.boards"),
        [cutoffTime](QSqlDatabase& database, DataCleanupResult* result) {
            QStringList boardCodes;
            if (!fetchBoardCodes(
                    database,
                    QString("SELECT br.board_code "
                            "FROM board_records br "
                            "WHERE br.event_time < ? "
                            "AND NOT EXISTS ("
                            "    SELECT 1 FROM point_records pr "
                            "    WHERE pr.board_code = br.board_code AND pr.end_time >= ?"
                            ") "
                            "ORDER BY br.event_time ASC "
                            "LIMIT %1")
                        .arg(kCleanupBatchSize),
                    {cutoffTime, cutoffTime},
                    &boardCodes,
                    &result->errorMessage)) {
                return false;
            }

            result->cutoffTime = cutoffTime;
            return deleteBoardsByCodes(database, boardCodes, result, &result->errorMessage);
        });
}

DataCleanupResult deleteOrphanOldPointsBatch(const DatabaseSettings& settings, const QDateTime& cutoffTime) {
    return executeCleanupTransaction(
        settings,
        QObject::tr("DataCleanup.production.orphans"),
        [cutoffTime](QSqlDatabase& database, DataCleanupResult* result) {
            int deleted = 0;
            if (!execDelete(
                    database,
                    QString("DELETE FROM point_records "
                            "WHERE end_time < ? "
                            "AND NOT EXISTS (SELECT 1 FROM board_records WHERE board_records.board_code = point_records.board_code) "
                            "LIMIT %1")
                        .arg(kCleanupBatchSize),
                    {cutoffTime},
                    &deleted,
                    &result->errorMessage)) {
                return false;
            }

            result->cutoffTime = cutoffTime;
            result->deletedPointRecords += deleted;
            if (deleted > 0) {
                result->processedBatches += 1;
            }
            return true;
        });
}

}  // namespace

DataCleanupResult DataCleanupService::cleanupSeedData(const DatabaseSettings& settings) {
    DataCleanupResult result;
    const QStringList boardCodes = seedBoardCodes();

    for (int offset = 0; offset < boardCodes.size(); offset += kCleanupBatchSize) {
        const DataCleanupResult batch =
            deleteSeedBoardsBatch(settings, boardCodes.mid(offset, kCleanupBatchSize));
        if (!batch.success) {
            return batch;
        }
        mergeCleanupResult(batch, &result);
    }

    const DataCleanupResult orphanBatch = deleteOrphanSeedPointsBatch(settings, boardCodes);
    if (!orphanBatch.success) {
        return orphanBatch;
    }
    mergeCleanupResult(orphanBatch, &result);

    result.success = true;
    return result;
}

DataCleanupResult DataCleanupService::cleanupProductionDataOlderThan(const DatabaseSettings& settings, int retentionDays) {
    DataCleanupResult result;
    if (retentionDays <= 0) {
        result.errorMessage = QObject::tr("Retention days must be greater than zero.");
        return result;
    }

    const DataCleanupResult databaseClock = fetchDatabaseCurrentTimeResult(settings);
    if (!databaseClock.success) {
        return databaseClock;
    }

    const QDateTime cutoffTime = databaseClock.cutoffTime.addDays(-retentionDays);
    result.cutoffTime = cutoffTime;
    result.retryCount += databaseClock.retryCount;

    while (true) {
        const DataCleanupResult batch = deleteEligibleProductionBoardsBatch(settings, cutoffTime);
        if (!batch.success) {
            return batch;
        }
        if (batch.deletedBoardRecords <= 0) {
            break;
        }
        mergeCleanupResult(batch, &result);
    }

    while (true) {
        const DataCleanupResult batch = deleteOrphanOldPointsBatch(settings, cutoffTime);
        if (!batch.success) {
            return batch;
        }
        if (batch.deletedPointRecords <= 0) {
            break;
        }
        mergeCleanupResult(batch, &result);
    }

    result.success = true;
    return result;
}

}  // namespace LaserSpc::Infrastructure
