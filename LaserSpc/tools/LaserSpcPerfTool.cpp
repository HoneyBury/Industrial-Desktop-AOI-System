#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QList>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTextStream>
#include <limits>

#include "domain/Models.h"
#include "infrastructure/DatabaseConnection.h"
#include "infrastructure/MySqlQueryBuilder.h"

namespace {

using LaserSpc::Domain::BadStatQuery;
using LaserSpc::Domain::BoardRecordQuery;
using LaserSpc::Domain::FilterCriteria;
using LaserSpc::Domain::Pagination;
using LaserSpc::Domain::PointRecordQuery;
using LaserSpc::Domain::SortOption;
using LaserSpc::Domain::SummaryQuery;
using LaserSpc::Infrastructure::DatabaseSettings;
using LaserSpc::Infrastructure::MySqlQueryBuilder;
using LaserSpc::Infrastructure::MySqlSqlStatement;

struct PerfConfig {
    QString mode;
    QString host;
    int port = 3306;
    QString connectOptions;
    QString adminDatabaseName;
    QString adminUser;
    QString adminPassword;
    QString targetDatabaseName;
    QString targetUser;
    QString targetPassword;
    int boardCount = 5000;
    int pointsPerBoard = 4;
    int rounds = 5;
    int topN = 10;
    bool appendData = false;
};

struct QueryCase {
    QString name;
    MySqlSqlStatement statement;
};

struct BenchmarkResult {
    QString name;
    int rounds = 0;
    int rowsRead = 0;
    double averageMs = 0.0;
    double minMs = 0.0;
    double maxMs = 0.0;
};

QString envOrDefault(const char* name, const QString& fallback) {
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() ? fallback : value;
}

int positiveOrDefault(const QString& value, int fallback) {
    bool ok = false;
    const int parsed = value.toInt(&ok);
    return ok && parsed > 0 ? parsed : fallback;
}

QString quoteIdentifier(const QString& value) {
    QString escaped = value;
    escaped.replace("`", "``");
    return "`" + escaped + "`";
}

bool executeRaw(QSqlDatabase& database, const QString& sql, QString* errorMessage) {
    QSqlQuery query(database);
    if (!query.exec(sql)) {
        if (errorMessage) {
            *errorMessage = query.lastError().text() + " | sql=" + sql;
        }
        return false;
    }
    return true;
}

bool indexExists(QSqlDatabase& database, const QString& tableName, const QString& indexName, QString* errorMessage) {
    QSqlQuery query(database);
    query.prepare(
        "SELECT COUNT(*) "
        "FROM information_schema.statistics "
        "WHERE table_schema = DATABASE() AND table_name = ? AND index_name = ?");
    query.addBindValue(tableName);
    query.addBindValue(indexName);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text() + " | table=" + tableName + " | index=" + indexName;
        }
        return false;
    }
    if (!query.next()) {
        if (errorMessage) {
            *errorMessage = "No rows returned when checking index existence.";
        }
        return false;
    }
    return query.value(0).toInt() > 0;
}

bool columnExists(QSqlDatabase& database, const QString& tableName, const QString& columnName, QString* errorMessage) {
    QSqlQuery query(database);
    query.prepare(
        "SELECT COUNT(*) "
        "FROM information_schema.columns "
        "WHERE table_schema = DATABASE() AND table_name = ? AND column_name = ?");
    query.addBindValue(tableName);
    query.addBindValue(columnName);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text() + " | table=" + tableName + " | column=" + columnName;
        }
        return false;
    }
    if (!query.next()) {
        if (errorMessage) {
            *errorMessage = "No rows returned when checking column existence.";
        }
        return false;
    }
    return query.value(0).toInt() > 0;
}

bool ensureColumn(QSqlDatabase& database,
                  const QString& tableName,
                  const QString& columnName,
                  const QString& ddl,
                  QString* errorMessage) {
    const bool exists = columnExists(database, tableName, columnName, errorMessage);
    if (!exists && errorMessage && !errorMessage->isEmpty()) {
        return false;
    }
    if (exists) {
        return true;
    }
    return executeRaw(database, ddl, errorMessage);
}

bool ensureIndex(QSqlDatabase& database,
                 const QString& tableName,
                 const QString& indexName,
                 const QString& ddl,
                 QString* errorMessage) {
    const bool exists = indexExists(database, tableName, indexName, errorMessage);
    if (!exists && errorMessage && !errorMessage->isEmpty()) {
        return false;
    }
    if (exists) {
        return true;
    }
    return executeRaw(database, ddl, errorMessage);
}

bool executePrepared(QSqlDatabase& database,
                     const MySqlSqlStatement& statement,
                     QString* errorMessage,
                     int* rowsRead = nullptr) {
    QSqlQuery query(database);
    query.prepare(statement.sql);
    for (const QVariant& value : statement.values) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text() + " | sql=" + statement.sql.simplified();
        }
        return false;
    }

    int rows = 0;
    while (query.next()) {
        ++rows;
    }

    if (rowsRead) {
        *rowsRead = rows;
    }
    return true;
}

bool queryScalar(QSqlDatabase& database, const QString& sql, qint64* value, QString* errorMessage) {
    QSqlQuery query(database);
    if (!query.exec(sql)) {
        if (errorMessage) {
            *errorMessage = query.lastError().text() + " | sql=" + sql;
        }
        return false;
    }
    if (!query.next()) {
        if (errorMessage) {
            *errorMessage = "No rows returned for scalar query: " + sql;
        }
        return false;
    }
    *value = query.value(0).toLongLong();
    return true;
}

DatabaseSettings makeSettings(const QString& host,
                              int port,
                              const QString& databaseName,
                              const QString& user,
                              const QString& password,
                              const QString& connectOptions) {
    DatabaseSettings settings;
    settings.host = host;
    settings.port = port;
    settings.databaseName = databaseName;
    settings.userName = user;
    settings.password = password;
    settings.connectOptions = connectOptions;
    return settings;
}

QSqlDatabase openDatabase(const DatabaseSettings& settings, QTextStream& err, const QString& purpose) {
    auto databaseLease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(settings);
    if (!databaseLease.isOpen()) {
        err << purpose << " failed: " << databaseLease.lastError() << "\n";
        return {};
    }
    return databaseLease.database();
}

bool ensureDatabase(const PerfConfig& config, QTextStream& out, QTextStream& err) {
    auto admin = openDatabase(makeSettings(config.host,
                                           config.port,
                                           config.adminDatabaseName,
                                           config.adminUser,
                                           config.adminPassword,
                                           config.connectOptions),
                              err,
                              "Open admin database");
    if (!admin.isOpen()) {
        return false;
    }

    QString errorMessage;
    const QString createDatabaseSql =
        "CREATE DATABASE IF NOT EXISTS " + quoteIdentifier(config.targetDatabaseName) +
        " CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci";
    if (!executeRaw(admin, createDatabaseSql, &errorMessage)) {
        err << "Create database failed: " << errorMessage << "\n";
        return false;
    }

    out << "Database ready: " << config.targetDatabaseName << "\n";
    return true;
}

bool ensureSchema(const PerfConfig& config, QTextStream& out, QTextStream& err) {
    auto database = openDatabase(makeSettings(config.host,
                                              config.port,
                                              config.targetDatabaseName,
                                              config.targetUser,
                                              config.targetPassword,
                                              config.connectOptions),
                                 err,
                                 "Open target database");
    if (!database.isOpen()) {
        return false;
    }

    QString errorMessage;
    const QStringList statements = {
        "CREATE TABLE IF NOT EXISTS board_records ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "board_code VARCHAR(64) NOT NULL,"
        "result VARCHAR(8) NOT NULL,"
        "line_name VARCHAR(32) NOT NULL,"
        "program_name VARCHAR(64) NOT NULL,"
        "device_name VARCHAR(64) NOT NULL,"
        "operator_name VARCHAR(64) NOT NULL,"
        "event_time DATETIME NOT NULL,"
        "UNIQUE KEY uk_board_code (board_code),"
        "KEY idx_board_event_time (event_time),"
        "KEY idx_board_line_program_device (line_name, program_name, device_name),"
        "KEY idx_board_result (result)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci",
        "CREATE TABLE IF NOT EXISTS point_records ("
        "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
        "board_code VARCHAR(64) NOT NULL,"
        "point_name VARCHAR(64) NOT NULL,"
        "result VARCHAR(8) NOT NULL,"
        "read_grade VARCHAR(16) NOT NULL,"
        "laser_content VARCHAR(255) NOT NULL DEFAULT '',"
        "read_code_content VARCHAR(255) NOT NULL DEFAULT '',"
        "is_laser TINYINT(1) NOT NULL DEFAULT 0,"
        "is_read_code TINYINT(1) NOT NULL DEFAULT 0,"
        "line_name VARCHAR(32) NOT NULL,"
        "program_name VARCHAR(64) NOT NULL,"
        "device_name VARCHAR(64) NOT NULL,"
        "start_time DATETIME NOT NULL,"
        "end_time DATETIME NOT NULL,"
        "detail_json_path VARCHAR(512) NOT NULL DEFAULT '',"
        "UNIQUE KEY uk_point_board_code_name (board_code, point_name),"
        "KEY idx_point_board_code (board_code),"
        "KEY idx_point_end_time (end_time),"
        "KEY idx_point_line_program_device (line_name, program_name, device_name),"
        "KEY idx_point_result_grade (result, read_grade),"
        "KEY idx_point_laser_content (laser_content(191)),"
        "KEY idx_point_read_code_content (read_code_content(191)),"
        "CONSTRAINT fk_point_board_code FOREIGN KEY (board_code) REFERENCES board_records(board_code) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci"
    };

    for (const QString& statement : statements) {
        if (!executeRaw(database, statement, &errorMessage)) {
            err << "Ensure schema failed: " << errorMessage << "\n";
            return false;
        }
    }

    const struct ColumnSpec {
        const char* tableName;
        const char* columnName;
        const char* ddl;
    } columnSpecs[] = {
        {"point_records", "laser_content",
         "ALTER TABLE point_records ADD COLUMN laser_content VARCHAR(255) NOT NULL DEFAULT '' AFTER read_grade"},
        {"point_records", "read_code_content",
         "ALTER TABLE point_records ADD COLUMN read_code_content VARCHAR(255) NOT NULL DEFAULT '' AFTER laser_content"},
        {"point_records", "is_laser",
         "ALTER TABLE point_records ADD COLUMN is_laser TINYINT(1) NOT NULL DEFAULT 0 AFTER read_code_content"},
        {"point_records", "is_read_code",
         "ALTER TABLE point_records ADD COLUMN is_read_code TINYINT(1) NOT NULL DEFAULT 0 AFTER is_laser"},
        {"point_records", "detail_json_path",
         "ALTER TABLE point_records ADD COLUMN detail_json_path VARCHAR(512) NOT NULL DEFAULT '' AFTER end_time"}
    };

    for (const auto& columnSpec : columnSpecs) {
        errorMessage.clear();
        if (!ensureColumn(database, columnSpec.tableName, columnSpec.columnName, columnSpec.ddl, &errorMessage)) {
            err << "Ensure column failed: " << errorMessage << "\n";
            return false;
        }
    }

    if (!executeRaw(database,
                    "ALTER TABLE point_records MODIFY COLUMN is_laser TINYINT(1) NOT NULL DEFAULT 0",
                    &errorMessage)) {
        err << "Normalize point_records.is_laser default failed: " << errorMessage << "\n";
        return false;
    }

    const struct IndexSpec {
        const char* tableName;
        const char* indexName;
        const char* ddl;
    } indexSpecs[] = {
        {"board_records", "idx_board_program_name",
         "ALTER TABLE board_records ADD INDEX idx_board_program_name (program_name)"},
        {"board_records", "idx_board_device_name",
         "ALTER TABLE board_records ADD INDEX idx_board_device_name (device_name)"},
        {"board_records", "idx_board_line_event",
         "ALTER TABLE board_records ADD INDEX idx_board_line_event (line_name, event_time)"},
        {"board_records", "idx_board_result_line_event",
         "ALTER TABLE board_records ADD INDEX idx_board_result_line_event (result, line_name, event_time)"},
        {"point_records", "uk_point_board_code_name",
         "ALTER TABLE point_records ADD CONSTRAINT uk_point_board_code_name UNIQUE (board_code, point_name)"},
        {"point_records", "idx_point_line_end_time",
         "ALTER TABLE point_records ADD INDEX idx_point_line_end_time (line_name, end_time)"},
        {"point_records", "idx_point_laser_content",
         "ALTER TABLE point_records ADD INDEX idx_point_laser_content (laser_content(191))"},
        {"point_records", "idx_point_read_code_content",
         "ALTER TABLE point_records ADD INDEX idx_point_read_code_content (read_code_content(191))"},
        {"point_records", "idx_point_result_line_end_time",
         "ALTER TABLE point_records ADD INDEX idx_point_result_line_end_time (result, line_name, end_time)"}
    };

    for (const auto& indexSpec : indexSpecs) {
        errorMessage.clear();
        if (!ensureIndex(database, indexSpec.tableName, indexSpec.indexName, indexSpec.ddl, &errorMessage)) {
            err << "Ensure index failed: " << errorMessage << "\n";
            return false;
        }
    }

    out << "Schema ready.\n";
    return true;
}

FilterCriteria buildWindowFilter(const PerfConfig& config) {
    FilterCriteria filter;
    const QDateTime end = QDateTime::currentDateTime().addSecs(60);
    const int secondsBack = qMax(1, config.boardCount / 3 + 600);
    filter.beginTime = end.addSecs(-secondsBack);
    filter.endTime = end;
    filter.lineName = "L2";
    filter.keyword = "PERF";
    return filter;
}

QList<QueryCase> buildQueryCases(const PerfConfig& config) {
    QList<QueryCase> cases;

    SummaryQuery summary;
    summary.filter = buildWindowFilter(config);
    summary.pagination = Pagination{1, 20};
    summary.sort = SortOption{"yieldRate", Qt::DescendingOrder};

    BadStatQuery bad;
    bad.filter = buildWindowFilter(config);
    bad.topN = config.topN;
    bad.filter.result.clear();
    bad.filter.keyword.clear();

    BoardRecordQuery boards;
    boards.filter = buildWindowFilter(config);
    boards.filter.result = "NG";
    boards.pagination = Pagination{1, 50};
    boards.sort = SortOption{"eventTime", Qt::DescendingOrder};

    PointRecordQuery points;
    points.filter = buildWindowFilter(config);
    points.pagination = Pagination{1, 100};
    points.sort = SortOption{"endTime", Qt::DescendingOrder};

    cases.append({"filterOptions.line_name", MySqlQueryBuilder::buildFilterOptionsStatement("line_name")});
    cases.append({"filterOptions.program_name", MySqlQueryBuilder::buildFilterOptionsStatement("program_name")});
    cases.append({"filterOptions.device_name", MySqlQueryBuilder::buildFilterOptionsStatement("device_name")});
    cases.append({"summary.metrics", MySqlQueryBuilder::buildSummaryMetricsStatement(summary)});
    cases.append({"summary.count", MySqlQueryBuilder::buildSummaryCountStatement(summary)});
    cases.append({"summary.rows", MySqlQueryBuilder::buildSummaryRowsStatement(summary)});
    cases.append({"badPoint.total", MySqlQueryBuilder::buildBadPointTotalStatement(bad)});
    cases.append({"badPoint.rows", MySqlQueryBuilder::buildBadPointStatsStatement(bad)});
    cases.append({"grade.total", MySqlQueryBuilder::buildGradeTotalStatement(bad)});
    cases.append({"grade.rows", MySqlQueryBuilder::buildGradeStatsStatement(bad)});
    cases.append({"board.count", MySqlQueryBuilder::buildBoardCountStatement(boards)});
    cases.append({"board.rows", MySqlQueryBuilder::buildBoardRowsStatement(boards)});
    cases.append({"point.count", MySqlQueryBuilder::buildPointCountStatement(points)});
    cases.append({"point.rows", MySqlQueryBuilder::buildPointRowsStatement(points)});

    return cases;
}

bool seedData(const PerfConfig& config, QTextStream& out, QTextStream& err) {
    auto database = openDatabase(makeSettings(config.host,
                                              config.port,
                                              config.targetDatabaseName,
                                              config.targetUser,
                                              config.targetPassword,
                                              config.connectOptions),
                                 err,
                                 "Open target database");
    if (!database.isOpen()) {
        return false;
    }

    QString errorMessage;
    if (!config.appendData) {
        if (!executeRaw(database, "DELETE FROM point_records", &errorMessage) ||
            !executeRaw(database, "DELETE FROM board_records", &errorMessage)) {
            err << "Clean old benchmark data failed: " << errorMessage << "\n";
            return false;
        }
    }

    const QStringList lines = {"L1", "L2", "L3", "L4"};
    const QStringList programs = {"Program-A", "Program-B", "Program-C", "Program-D", "Program-E"};
    const QStringList devices = {"Laser-01", "Laser-02", "Laser-03", "Laser-04", "Laser-05"};
    const QStringList operators = {"Alice", "Bob", "Chris", "Diana", "Eric", "Fiona"};
    const QStringList pointNames = {"Point-Read", "Point-Mark", "Point-Check", "Point-Verify", "Point-Final"};
    const QStringList okGrades = {"A", "A", "B"};
    const QStringList ngGrades = {"C", "D"};

    QSqlQuery boardInsert(database);
    boardInsert.prepare("INSERT INTO board_records "
                        "(board_code, result, line_name, program_name, device_name, operator_name, event_time) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?)");

    QSqlQuery pointInsert(database);
    pointInsert.prepare("INSERT INTO point_records "
                        "(board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code, "
                        "line_name, program_name, device_name, start_time, end_time, detail_json_path) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    if (!database.transaction()) {
        err << "Failed to start transaction: " << database.lastError().text() << "\n";
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    const QDateTime baseTime = QDateTime::currentDateTime().addSecs(-config.boardCount * 3);
    qint64 pointCounter = 0;

    for (int boardIndex = 0; boardIndex < config.boardCount; ++boardIndex) {
        const QString line = lines.at(boardIndex % lines.size());
        const QString program = programs.at(boardIndex % programs.size());
        const QString device = devices.at(boardIndex % devices.size());
        const QString operatorName = operators.at(boardIndex % operators.size());
        const QString boardCode = QString("PERF-%1-%2")
                                      .arg(QDateTime::currentDateTime().toString("yyyyMMdd"))
                                      .arg(boardIndex + 1, 7, 10, QChar('0'));
        const bool boardNg = ((boardIndex + 1) % 7 == 0) || ((boardIndex + 1) % 19 == 0);
        const QString boardResult = boardNg ? "NG" : "OK";
        const QDateTime eventTime = baseTime.addSecs(boardIndex * 3);

        boardInsert.bindValue(0, boardCode);
        boardInsert.bindValue(1, boardResult);
        boardInsert.bindValue(2, line);
        boardInsert.bindValue(3, program);
        boardInsert.bindValue(4, device);
        boardInsert.bindValue(5, operatorName);
        boardInsert.bindValue(6, eventTime);
        if (!boardInsert.exec()) {
            database.rollback();
            err << "Insert board record failed: " << boardInsert.lastError().text() << "\n";
            return false;
        }

        const int ngPointIndex = boardNg ? (boardIndex % qMax(1, config.pointsPerBoard)) : -1;
        for (int pointIndex = 0; pointIndex < config.pointsPerBoard; ++pointIndex) {
            const bool pointNg = pointIndex == ngPointIndex;
            const QString pointResult = pointNg ? "NG" : "OK";
            const QString gradeSource = pointNg ? ngGrades.at(pointIndex % ngGrades.size())
                                                : okGrades.at((boardIndex + pointIndex) % okGrades.size());
            const QString pointName =
                pointNames.at((boardIndex + pointIndex) % pointNames.size()) + QString("-%1").arg(pointIndex + 1);
            const QString laserContent = pointName + "-LASER";
            const QString readCodeContent = pointNg ? QStringLiteral("null")
                                                    : QString("READ-%1-%2").arg(boardIndex + 1).arg(pointIndex + 1);
            const QDateTime startTime = eventTime.addSecs(-5 - pointIndex);
            const QDateTime endTime = eventTime.addSecs(pointIndex);

            pointInsert.bindValue(0, boardCode);
            pointInsert.bindValue(1, pointName);
            pointInsert.bindValue(2, pointResult);
            pointInsert.bindValue(3, gradeSource);
            pointInsert.bindValue(4, laserContent);
            pointInsert.bindValue(5, readCodeContent);
            pointInsert.bindValue(6, true);
            pointInsert.bindValue(7, readCodeContent != QStringLiteral("null"));
            pointInsert.bindValue(8, line);
            pointInsert.bindValue(9, program);
            pointInsert.bindValue(10, device);
            pointInsert.bindValue(11, startTime);
            pointInsert.bindValue(12, endTime);
            pointInsert.bindValue(13, QStringLiteral(""));
            if (!pointInsert.exec()) {
                database.rollback();
                err << "Insert point record failed: " << pointInsert.lastError().text() << "\n";
                return false;
            }

            ++pointCounter;
        }
    }

    if (!database.commit()) {
        err << "Commit benchmark seed failed: " << database.lastError().text() << "\n";
        return false;
    }

    out << "Seed completed in " << timer.elapsed() << " ms"
        << " | boards=" << config.boardCount
        << " | points=" << pointCounter
        << " | append=" << (config.appendData ? "true" : "false") << "\n";
    return true;
}

BenchmarkResult benchmarkStatement(QSqlDatabase& database,
                                   const QueryCase& queryCase,
                                   int rounds,
                                   QTextStream& err) {
    BenchmarkResult result;
    result.name = queryCase.name;
    result.rounds = rounds;

    qint64 totalNs = 0;
    qint64 minNs = std::numeric_limits<qint64>::max();
    qint64 maxNs = 0;

    for (int round = 0; round < rounds; ++round) {
        int rowsRead = 0;
        QString errorMessage;
        QElapsedTimer timer;
        timer.start();
        if (!executePrepared(database, queryCase.statement, &errorMessage, &rowsRead)) {
            err << "Benchmark failed for " << queryCase.name << ": " << errorMessage << "\n";
            result.rounds = round;
            return result;
        }

        const qint64 elapsedNs = timer.nsecsElapsed();
        totalNs += elapsedNs;
        minNs = qMin(minNs, elapsedNs);
        maxNs = qMax(maxNs, elapsedNs);
        result.rowsRead = rowsRead;
    }

    result.averageMs = static_cast<double>(totalNs) / rounds / 1000000.0;
    result.minMs = static_cast<double>(minNs) / 1000000.0;
    result.maxMs = static_cast<double>(maxNs) / 1000000.0;
    return result;
}

bool runBenchmarks(const PerfConfig& config, QTextStream& out, QTextStream& err) {
    auto database = openDatabase(makeSettings(config.host,
                                              config.port,
                                              config.targetDatabaseName,
                                              config.targetUser,
                                              config.targetPassword,
                                              config.connectOptions),
                                 err,
                                 "Open target database");
    if (!database.isOpen()) {
        return false;
    }

    out << "Benchmark rounds: " << config.rounds << "\n";
    const auto cases = buildQueryCases(config);
    for (const QueryCase& queryCase : cases) {
        const auto result = benchmarkStatement(database, queryCase, config.rounds, err);
        if (result.rounds != config.rounds) {
            return false;
        }
        out << queryCase.name
            << " | avg=" << QString::number(result.averageMs, 'f', 3)
            << " ms | min=" << QString::number(result.minMs, 'f', 3)
            << " ms | max=" << QString::number(result.maxMs, 'f', 3)
            << " ms | rows=" << result.rowsRead << "\n";
    }

    return true;
}

QString explainHeuristic(const QSqlQuery& query, const QSqlRecord& record) {
    const int typeIndex = record.indexOf("type");
    const int keyIndex = record.indexOf("key");
    const int rowsIndex = record.indexOf("rows");
    const int extraIndex = record.indexOf("Extra");

    const QString accessType = typeIndex >= 0 ? query.value(typeIndex).toString() : QString();
    const QString key = keyIndex >= 0 ? query.value(keyIndex).toString() : QString();
    const qlonglong scannedRows = rowsIndex >= 0 ? query.value(rowsIndex).toLongLong() : 0;
    const QString extra = extraIndex >= 0 ? query.value(extraIndex).toString() : QString();

    if (accessType == "ALL") {
        return "risk=full_scan";
    }
    if (key.isEmpty()) {
        return "risk=no_index";
    }
    if (scannedRows > 100000) {
        return "risk=high_rows";
    }
    if (extra.contains("filesort", Qt::CaseInsensitive)) {
        return "risk=filesort";
    }
    return "risk=low";
}

bool runExplain(const PerfConfig& config, QTextStream& out, QTextStream& err) {
    auto database = openDatabase(makeSettings(config.host,
                                              config.port,
                                              config.targetDatabaseName,
                                              config.targetUser,
                                              config.targetPassword,
                                              config.connectOptions),
                                 err,
                                 "Open target database");
    if (!database.isOpen()) {
        return false;
    }

    const auto cases = buildQueryCases(config);
    for (const QueryCase& queryCase : cases) {
        QSqlQuery query(database);
        query.prepare("EXPLAIN FORMAT=TRADITIONAL " + queryCase.statement.sql);
        for (const QVariant& value : queryCase.statement.values) {
            query.addBindValue(value);
        }

        if (!query.exec()) {
            err << "EXPLAIN failed for " << queryCase.name << ": " << query.lastError().text() << "\n";
            return false;
        }

        out << "[EXPLAIN] " << queryCase.name << "\n";
        const QSqlRecord record = query.record();
        while (query.next()) {
            QStringList columns;
            for (int i = 0; i < record.count(); ++i) {
                columns.append(record.fieldName(i) + "=" + query.value(i).toString());
            }
            out << "  " << columns.join(", ") << ", " << explainHeuristic(query, record) << "\n";
        }
    }

    return true;
}

bool printDatasetCounts(const PerfConfig& config, QTextStream& out, QTextStream& err) {
    auto database = openDatabase(makeSettings(config.host,
                                              config.port,
                                              config.targetDatabaseName,
                                              config.targetUser,
                                              config.targetPassword,
                                              config.connectOptions),
                                 err,
                                 "Open target database");
    if (!database.isOpen()) {
        return false;
    }

    QString errorMessage;
    qint64 boardCount = 0;
    qint64 pointCount = 0;
    if (!queryScalar(database, "SELECT COUNT(*) FROM board_records", &boardCount, &errorMessage) ||
        !queryScalar(database, "SELECT COUNT(*) FROM point_records", &pointCount, &errorMessage)) {
        err << "Query dataset counts failed: " << errorMessage << "\n";
        return false;
    }

    out << "Dataset counts | boards=" << boardCount << " | points=" << pointCount << "\n";
    return true;
}

PerfConfig parseConfig(QCoreApplication& app) {
    QCommandLineParser parser;
    parser.setApplicationDescription("LaserSpc MySQL benchmark and EXPLAIN tool");
    parser.addHelpOption();

    parser.addOption({{"m", "mode"}, "seed, bench, explain, all", "mode", "all"});
    parser.addOption({{"H", "host"}, "MySQL host", "host", envOrDefault("LASERSPC_DB_HOST", "127.0.0.1")});
    parser.addOption({{"P", "port"}, "MySQL port", "port", envOrDefault("LASERSPC_DB_PORT", "3306")});
    parser.addOption({"connect-options",
                      "QMYSQL connect options",
                      "connectOptions",
                      envOrDefault("LASERSPC_DB_CONNECT_OPTIONS", QString())});
    parser.addOption({"admin-db", "Admin database name", "adminDb", envOrDefault("LASERSPC_ADMIN_DB", "mysql")});
    parser.addOption({"admin-user", "Admin user", "adminUser", envOrDefault("LASERSPC_ADMIN_USER", envOrDefault("LASERSPC_DB_USER", "root"))});
    parser.addOption({"admin-password",
                      "Admin password",
                      "adminPassword",
                      envOrDefault("LASERSPC_ADMIN_PASSWORD", envOrDefault("LASERSPC_DB_PASSWORD", QString()))});
    parser.addOption({"db-name", "Benchmark database name", "dbName", envOrDefault("LASERSPC_PERF_DB_NAME", "laser_spc_perf")});
    parser.addOption({"target-user",
                      "Benchmark database user",
                      "targetUser",
                      envOrDefault("LASERSPC_TARGET_USER",
                                   envOrDefault("LASERSPC_ADMIN_USER",
                                                envOrDefault("LASERSPC_DB_USER", "root")))});
    parser.addOption({"target-password",
                      "Benchmark database password",
                      "targetPassword",
                      envOrDefault("LASERSPC_TARGET_PASSWORD",
                                   envOrDefault("LASERSPC_ADMIN_PASSWORD",
                                                envOrDefault("LASERSPC_DB_PASSWORD", QString())))});
    parser.addOption({"boards", "Number of board records to seed", "boards", "5000"});
    parser.addOption({"points-per-board", "Point records per board", "pointsPerBoard", "4"});
    parser.addOption({"rounds", "Benchmark rounds per query", "rounds", "5"});
    parser.addOption({"top-n", "Top N bad points for the benchmark query", "topN", "10"});
    parser.addOption({"append", "Append benchmark data instead of clearing existing rows"});

    parser.process(app);

    PerfConfig config;
    config.mode = parser.value("mode").trimmed().toLower();
    if (config.mode.isEmpty()) {
        config.mode = "all";
    }
    config.host = parser.value("host").trimmed();
    config.port = positiveOrDefault(parser.value("port"), 3306);
    config.connectOptions = parser.value("connect-options");
    config.adminDatabaseName = parser.value("admin-db").trimmed();
    config.adminUser = parser.value("admin-user").trimmed();
    config.adminPassword = parser.value("admin-password");
    config.targetDatabaseName = parser.value("db-name").trimmed();
    config.targetUser = parser.value("target-user").trimmed();
    config.targetPassword = parser.value("target-password");
    config.boardCount = positiveOrDefault(parser.value("boards"), 5000);
    config.pointsPerBoard = positiveOrDefault(parser.value("points-per-board"), 4);
    config.rounds = positiveOrDefault(parser.value("rounds"), 5);
    config.topN = positiveOrDefault(parser.value("top-n"), 10);
    config.appendData = parser.isSet("append");
    return config;
}

bool modeIncludes(const QString& mode, const QString& step) {
    return mode == "all" || mode == step;
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const PerfConfig config = parseConfig(app);
    out << "LaserSpcPerfTool"
        << " | mode=" << config.mode
        << " | host=" << config.host
        << " | port=" << config.port
        << " | db=" << config.targetDatabaseName
        << "\n";

    if (!ensureDatabase(config, out, err) || !ensureSchema(config, out, err)) {
        return 1;
    }

    if (modeIncludes(config.mode, "seed")) {
        if (!seedData(config, out, err)) {
            return 1;
        }
    }

    if (!printDatasetCounts(config, out, err)) {
        return 1;
    }

    if (modeIncludes(config.mode, "bench")) {
        if (!runBenchmarks(config, out, err)) {
            return 1;
        }
    }

    if (modeIncludes(config.mode, "explain")) {
        if (!runExplain(config, out, err)) {
            return 1;
        }
    }

    return 0;
}
