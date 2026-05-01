#include "infrastructure/MySqlSpcWriteRepository.h"

#include <QElapsedTimer>
#include <QSqlError>
#include <QSqlQuery>
#include <QtGlobal>

#include "infrastructure/Logger.h"
#include "infrastructure/MySqlTransactionExecutor.h"

namespace LaserSpc::Infrastructure {

namespace {

QString summarizeSql(const QString& sql) {
    const QString compact = sql.simplified();
    return compact.size() > 120 ? compact.left(117) + "..." : compact;
}

QString databaseTarget(const DatabaseSettings& settings) {
    return QString("%1:%2/%3 user=%4")
        .arg(settings.host, QString::number(settings.port), settings.databaseName, settings.userName);
}

QString summarizeValue(const QVariant& value) {
    if (!value.isValid() || value.isNull()) {
        return QStringLiteral("<null>");
    }
    QString text = value.toString().simplified();
    if (text.isEmpty()) {
        text = QStringLiteral("\"\"");
    }
    return text.size() > 48 ? text.left(45) + "..." : text;
}

QString summarizeBinds(const QList<QVariant>& values) {
    QStringList parts;
    const int limit = qMin(values.size(), 10);
    parts.reserve(limit + 1);
    for (int index = 0; index < limit; ++index) {
        parts.append(QStringLiteral("%1=%2").arg(index).arg(summarizeValue(values.at(index))));
    }
    if (values.size() > limit) {
        parts.append(QStringLiteral("... total=%1").arg(values.size()));
    }
    return parts.join(QStringLiteral(", "));
}

QString classifySqlFailure(const QSqlError& error) {
    const QString text = (error.text() + " " + error.databaseText() + " " + error.driverText()).trimmed().toLower();
    const QString nativeCode = error.nativeErrorCode().trimmed();
    if (nativeCode == QStringLiteral("1062") || text.contains("duplicate entry")) {
        return QStringLiteral("duplicate_key");
    }
    if (nativeCode == QStringLiteral("1048") || text.contains("cannot be null") || text.contains("doesn't have a default value")) {
        return QStringLiteral("not_null_violation");
    }
    if (nativeCode == QStringLiteral("1452") || text.contains("foreign key constraint fails")) {
        return QStringLiteral("foreign_key_violation");
    }
    if (nativeCode == QStringLiteral("1146") || text.contains("doesn't exist")) {
        return QStringLiteral("table_not_found");
    }
    if (nativeCode == QStringLiteral("1049") || text.contains("unknown database")) {
        return QStringLiteral("database_not_found");
    }
    if (nativeCode == QStringLiteral("1045") || text.contains("access denied")) {
        return QStringLiteral("authentication_failed");
    }
    if (text.contains("driver not loaded")) {
        return QStringLiteral("driver_unavailable");
    }
    if (text.contains("not open")) {
        return QStringLiteral("connection_not_open");
    }
    if (text.contains("syntax")) {
        return QStringLiteral("sql_syntax_error");
    }
    if (text.contains("timeout")) {
        return QStringLiteral("timeout");
    }
    return QStringLiteral("sql_exec_failed");
}

QVariant sqlDateTimeValue(const QDateTime& value) {
    return value.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

constexpr int kBatchInsertChunkSize = 200;

QString pointInsertValuesClause(int rowCount) {
    QStringList groups;
    groups.reserve(rowCount);
    for (int index = 0; index < rowCount; ++index) {
        groups.append("(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    }
    return groups.join(", ");
}

}  // namespace

MySqlSpcWriteRepository::MySqlSpcWriteRepository(DatabaseSettings settings) : m_settings(std::move(settings)) {}

QString MySqlSpcWriteRepository::lastError() const {
    return m_lastError;
}

DatabaseConnection::ConnectionLease MySqlSpcWriteRepository::database() const {
    m_lastError.clear();
    auto lease = DatabaseConnection::acquireMySql(m_settings);
    if (!lease.isOpen()) {
        m_lastError = lease.lastError();
        Logger::error("WriteRepository.acquireDatabase failed | target=" + databaseTarget(m_settings) +
                      " | error=" + (m_lastError.isEmpty() ? QStringLiteral("unknown_open_error") : m_lastError));
    } else {
        Logger::info("WriteRepository.acquireDatabase succeeded | target=" + databaseTarget(m_settings) +
                     " | connectionName=" + lease.database().connectionName());
    }
    return lease;
}

bool MySqlSpcWriteRepository::exec(QSqlQuery& query,
                                   const QString& sql,
                                   const QList<QVariant>& values,
                                   const QString& label) const {
    query.prepare(sql);
    for (const auto& value : values) {
        query.addBindValue(value);
    }

    QElapsedTimer timer;
    timer.start();
    Logger::info(QString("%1 prepare | target=%2 | sql=%3 | binds=%4")
                     .arg(label, databaseTarget(m_settings), summarizeSql(sql), summarizeBinds(values)));
    if (!query.exec()) {
        const QSqlError error = query.lastError();
        m_lastError = error.text();
        Logger::error(QString("%1 failed | target=%2 | reason=%3 | native=%4 | error=%5 | sql=%6 | binds=%7")
                          .arg(label,
                               databaseTarget(m_settings),
                               classifySqlFailure(error),
                               error.nativeErrorCode(),
                               error.text(),
                               summarizeSql(sql),
                               summarizeBinds(values)));
        return false;
    }

    Logger::info(QString("%1 success | target=%2 | rowsAffected=%3 | sql=%4 | binds=%5")
                     .arg(label,
                          databaseTarget(m_settings),
                          QString::number(query.numRowsAffected()),
                          summarizeSql(sql),
                          summarizeBinds(values)));
    Logger::perf(label, timer.elapsed(), QString("binds=%1").arg(values.size()));
    return true;
}

bool MySqlSpcWriteRepository::upsertBoardRecord(const LaserSpc::Domain::BoardRecordRow& row) {
    auto lease = database();
    if (!lease.isOpen()) {
        return false;
    }

    QSqlQuery query(lease.database());
    return exec(query,
                "INSERT INTO board_records "
                "(board_code, result, line_name, program_name, device_name, operator_name, event_time) "
                "VALUES (?, ?, ?, ?, ?, ?, ?) "
                "ON DUPLICATE KEY UPDATE "
                "result = VALUES(result), "
                "line_name = VALUES(line_name), "
                "program_name = VALUES(program_name), "
                "device_name = VALUES(device_name), "
                "operator_name = VALUES(operator_name), "
                "event_time = VALUES(event_time)",
                {row.boardCode,
                 row.result,
                 row.lineName,
                 row.programName,
                 row.deviceName,
                 row.operatorName,
                 sqlDateTimeValue(row.eventTime)},
                "WriteRepository.upsertBoardRecord");
}

bool MySqlSpcWriteRepository::insertPointRecord(const LaserSpc::Domain::PointRecordRow& row) {
    auto lease = database();
    if (!lease.isOpen()) {
        return false;
    }

    const QString normalizedReadCodeContent = row.readCodeContent.trimmed().isEmpty() ? QStringLiteral("null")
                                                                                      : row.readCodeContent;
    const QString normalizedLaserContent = row.laserContent.trimmed();
    const bool isReadCode = row.isReadCode || (normalizedReadCodeContent != QStringLiteral("null") &&
                                               !normalizedReadCodeContent.trimmed().isEmpty());

    QSqlQuery query(lease.database());
    return exec(query,
                "INSERT INTO point_records "
                "(board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code, line_name, program_name, device_name, start_time, end_time, detail_json_path) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                "ON DUPLICATE KEY UPDATE "
                "result = VALUES(result), "
                "read_grade = VALUES(read_grade), "
                "laser_content = VALUES(laser_content), "
                "read_code_content = VALUES(read_code_content), "
                "is_laser = VALUES(is_laser), "
                "is_read_code = VALUES(is_read_code), "
                "line_name = VALUES(line_name), "
                "program_name = VALUES(program_name), "
                "device_name = VALUES(device_name), "
                "start_time = VALUES(start_time), "
                "end_time = VALUES(end_time), "
                "detail_json_path = VALUES(detail_json_path)",
                {row.boardCode,
                 row.pointName,
                 row.result,
                 row.readGrade,
                 normalizedLaserContent,
                 normalizedReadCodeContent,
                 row.isLaser,
                 isReadCode,
                 row.lineName,
                 row.programName,
                 row.deviceName,
                 sqlDateTimeValue(row.startTime),
                 sqlDateTimeValue(row.endTime),
                 row.detailJsonPath},
                "WriteRepository.insertPointRecord");
}

bool MySqlSpcWriteRepository::replaceInspectionBatch(const LaserSpc::Domain::BoardRecordRow& board,
                                                     const QList<LaserSpc::Domain::PointRecordRow>& points) {
    auto lease = database();
    if (!lease.isOpen()) {
        return false;
    }

    QSqlDatabase db = lease.database();
    if (!MySqlTransactionExecutor::begin(db, QStringLiteral("WriteRepository.batch"), &m_lastError)) {
        Logger::error("WriteRepository.batch.beginTransaction failed | target=" + databaseTarget(m_settings) +
                      " | error=" + m_lastError);
        return false;
    }
    Logger::info("WriteRepository.batch.beginTransaction success | target=" + databaseTarget(m_settings) +
                 " | board=" + board.boardCode +
                 " | points=" + QString::number(points.size()));

    QSqlQuery query(db);
    if (!exec(query,
              "INSERT INTO board_records "
              "(board_code, result, line_name, program_name, device_name, operator_name, event_time) "
              "VALUES (?, ?, ?, ?, ?, ?, ?) "
              "ON DUPLICATE KEY UPDATE "
              "result = VALUES(result), "
              "line_name = VALUES(line_name), "
              "program_name = VALUES(program_name), "
              "device_name = VALUES(device_name), "
              "operator_name = VALUES(operator_name), "
              "event_time = VALUES(event_time)",
              {board.boardCode,
               board.result,
               board.lineName,
               board.programName,
               board.deviceName,
               board.operatorName,
               sqlDateTimeValue(board.eventTime)},
              "WriteRepository.batch.upsertBoard")) {
        Logger::warn("WriteRepository.batch.rollback | target=" + databaseTarget(m_settings) +
                     " | stage=upsertBoard | board=" + board.boardCode);
        MySqlTransactionExecutor::rollback(db, QStringLiteral("WriteRepository.batch"));
        return false;
    }

    if (!exec(query,
              "DELETE FROM point_records WHERE board_code = ?",
              {board.boardCode},
              "WriteRepository.batch.deletePoints")) {
        Logger::warn("WriteRepository.batch.rollback | target=" + databaseTarget(m_settings) +
                     " | stage=deletePoints | board=" + board.boardCode);
        MySqlTransactionExecutor::rollback(db, QStringLiteral("WriteRepository.batch"));
        return false;
    }

    for (int offset = 0; offset < points.size(); offset += kBatchInsertChunkSize) {
        const int chunkSize = qMin(kBatchInsertChunkSize, points.size() - offset);
        QList<QVariant> values;
        values.reserve(chunkSize * 14);
        for (int index = 0; index < chunkSize; ++index) {
            const auto& point = points.at(offset + index);
            const QString normalizedReadCodeContent = point.readCodeContent.trimmed().isEmpty()
                                                          ? QStringLiteral("null")
                                                          : point.readCodeContent;
            const bool isReadCode = point.isReadCode || (normalizedReadCodeContent != QStringLiteral("null") &&
                                                         !normalizedReadCodeContent.trimmed().isEmpty());
            values << point.boardCode
                   << point.pointName
                   << point.result
                   << point.readGrade
                   << point.laserContent
                   << normalizedReadCodeContent
                   << point.isLaser
                   << isReadCode
                   << point.lineName
                   << point.programName
                   << point.deviceName
                   << sqlDateTimeValue(point.startTime)
                   << sqlDateTimeValue(point.endTime)
                   << point.detailJsonPath;
        }

        if (!exec(query,
                  "INSERT INTO point_records "
                  "(board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code, line_name, program_name, device_name, start_time, end_time, detail_json_path) "
                  "VALUES " + pointInsertValuesClause(chunkSize),
                  values,
                  "WriteRepository.batch.insertPointChunk")) {
            Logger::warn("WriteRepository.batch.rollback | target=" + databaseTarget(m_settings) +
                         " | stage=insertPointChunk | board=" + board.boardCode +
                         " | chunkOffset=" + QString::number(offset) +
                         " | chunkSize=" + QString::number(chunkSize));
            MySqlTransactionExecutor::rollback(db, QStringLiteral("WriteRepository.batch"));
            return false;
        }
    }

    if (!MySqlTransactionExecutor::commit(db, QStringLiteral("WriteRepository.batch"), &m_lastError)) {
        Logger::error("WriteRepository.batch.commit failed | target=" + databaseTarget(m_settings) +
                      " | board=" + board.boardCode +
                      " | error=" + m_lastError);
        MySqlTransactionExecutor::rollback(db, QStringLiteral("WriteRepository.batch"));
        return false;
    }
    Logger::info("WriteRepository.batch.commit success | target=" + databaseTarget(m_settings) +
                 " | board=" + board.boardCode +
                 " | points=" + QString::number(points.size()));
    return true;
}

}  // namespace LaserSpc::Infrastructure
