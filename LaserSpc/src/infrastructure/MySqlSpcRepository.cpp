#include "infrastructure/MySqlSpcRepository.h"

#include <QElapsedTimer>
#include <QSqlError>
#include <QSqlQuery>

#include "infrastructure/Logger.h"
#include "ui/common/UiTextCatalog.h"

using LaserSpc::Domain::BadPointStatRow;
using LaserSpc::Domain::BadStatQuery;
using LaserSpc::Domain::BoardRecordQuery;
using LaserSpc::Domain::BoardRecordRow;
using LaserSpc::Domain::GradeStatRow;
using LaserSpc::Domain::MetricCardData;
using LaserSpc::Domain::PageResult;
using LaserSpc::Domain::PointRecordQuery;
using LaserSpc::Domain::PointRecordRow;
using LaserSpc::Domain::SummaryQuery;
using LaserSpc::Domain::SummaryRow;

namespace {

QString summarizeSql(const QString& sql) {
    const QString compact = sql.simplified();
    return compact.size() > 120 ? compact.left(117) + "..." : compact;
}

QString totalBoardsTitle() {
    return LaserSpc::Ui::TextCatalog::summaryMetricText(LaserSpc::Ui::TextCatalog::SummaryMetricTextKey::TotalBoardsTitle);
}

QString totalBoardsDesc() {
    return LaserSpc::Ui::TextCatalog::summaryMetricText(LaserSpc::Ui::TextCatalog::SummaryMetricTextKey::TotalBoardsDescription);
}

QString goodBoardsTitle() {
    return LaserSpc::Ui::TextCatalog::summaryMetricText(LaserSpc::Ui::TextCatalog::SummaryMetricTextKey::GoodBoardsTitle);
}

QString goodBoardsDesc() {
    return LaserSpc::Ui::TextCatalog::summaryMetricText(LaserSpc::Ui::TextCatalog::SummaryMetricTextKey::GoodBoardsDescription);
}

QString badBoardsTitle() {
    return LaserSpc::Ui::TextCatalog::summaryMetricText(LaserSpc::Ui::TextCatalog::SummaryMetricTextKey::BadBoardsTitle);
}

QString badBoardsDesc() {
    return LaserSpc::Ui::TextCatalog::summaryMetricText(LaserSpc::Ui::TextCatalog::SummaryMetricTextKey::BadBoardsDescription);
}

QString yieldRateTitle() {
    return LaserSpc::Ui::TextCatalog::summaryMetricText(LaserSpc::Ui::TextCatalog::SummaryMetricTextKey::YieldRateTitle);
}

QString yieldRateDesc() {
    return LaserSpc::Ui::TextCatalog::summaryMetricText(LaserSpc::Ui::TextCatalog::SummaryMetricTextKey::YieldRateDescription);
}

}  // namespace

namespace LaserSpc::Infrastructure {

MySqlSpcRepository::MySqlSpcRepository(DatabaseSettings settings) : m_settings(std::move(settings)) {}

DatabaseConnection::ConnectionLease MySqlSpcRepository::database() const {
    m_lastError.clear();
    auto lease = DatabaseConnection::acquireMySql(m_settings);
    if (!lease.isOpen()) {
        m_lastError = lease.lastError();
    }
    return lease;
}

bool MySqlSpcRepository::isAvailable() const {
    auto lease = database();
    return lease.isOpen();
}

QString MySqlSpcRepository::lastError() const {
    return m_lastError;
}

bool MySqlSpcRepository::executeStatement(QSqlQuery& query,
                                          const MySqlSqlStatement& statement,
                                          const QString& label) const {
    query.prepare(statement.sql);
    for (const QVariant& value : statement.values) {
        query.addBindValue(value);
    }

    QElapsedTimer timer;
    timer.start();
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        Logger::error(QString("%1 failed: %2 | sql=%3")
                          .arg(label, m_lastError, summarizeSql(statement.sql)));
        return false;
    }

    Logger::perf(label,
                 timer.elapsed(),
                 QString("binds=%1 sql=%2").arg(statement.values.size()).arg(summarizeSql(statement.sql)));
    return true;
}

LaserSpc::Domain::FilterOptions MySqlSpcRepository::fetchFilterOptions() const {
    m_lastError.clear();
    QElapsedTimer timer;
    timer.start();

    auto options = LaserSpc::Domain::FilterOptions{
        queryDistinctValues("line_name"),
        queryDistinctValues("program_name"),
        queryDistinctValues("device_name")
    };

    Logger::perf("Repository.fetchFilterOptions",
                 timer.elapsed(),
                 QString("lines=%1 programs=%2 devices=%3")
                     .arg(options.lineNames.size())
                     .arg(options.programNames.size())
                     .arg(options.deviceNames.size()));
    return options;
}

QStringList MySqlSpcRepository::queryDistinctValues(const QString& columnName) const {
    QStringList values;
    if (MySqlQueryBuilder::filterOptionColumn(columnName).isEmpty()) {
        m_lastError = "Unsupported filter option column: " + columnName;
        Logger::warn(m_lastError);
        return values;
    }

    auto lease = database();
    if (!lease.isOpen()) {
        return values;
    }

    QSqlQuery sql(lease.database());
    if (!executeStatement(sql,
                          MySqlQueryBuilder::buildFilterOptionsStatement(columnName),
                          QString("Repository.filterOptions.%1").arg(columnName))) {
        return values;
    }

    while (sql.next()) {
        values.append(sql.value(0).toString());
    }
    return values;
}

QList<MetricCardData> MySqlSpcRepository::fetchSummaryMetrics(const SummaryQuery& query) const {
    m_lastError.clear();
    QList<MetricCardData> metrics;
    auto lease = database();
    if (!lease.isOpen()) {
        return metrics;
    }

    QElapsedTimer timer;
    timer.start();

    QSqlQuery sql(lease.database());
    if (!executeStatement(sql,
                          MySqlQueryBuilder::buildSummaryMetricsStatement(query),
                          "Repository.fetchSummaryMetrics")) {
        return metrics;
    }

    int totalBoards = 0;
    int goodBoards = 0;
    int badBoards = 0;
    if (sql.next()) {
        totalBoards = sql.value("total_boards").toInt();
        goodBoards = sql.value("good_boards").toInt();
        badBoards = sql.value("bad_boards").toInt();
    }
    const double yieldRate = totalBoards == 0 ? 0.0 : static_cast<double>(goodBoards) * 100.0 / totalBoards;

    metrics << MetricCardData{totalBoardsTitle(), QString::number(totalBoards), totalBoardsDesc()}
            << MetricCardData{goodBoardsTitle(), QString::number(goodBoards), goodBoardsDesc()}
            << MetricCardData{badBoardsTitle(), QString::number(badBoards), badBoardsDesc()}
            << MetricCardData{yieldRateTitle(), MySqlQueryBuilder::formatPercent(yieldRate), yieldRateDesc()};

    Logger::perf("Repository.fetchSummaryMetrics",
                 timer.elapsed(),
                 QString("total=%1 good=%2 bad=%3")
                     .arg(totalBoards)
                     .arg(goodBoards)
                     .arg(badBoards));
    return metrics;
}

PageResult<SummaryRow> MySqlSpcRepository::fetchSummaryRows(const SummaryQuery& query) const {
    m_lastError.clear();
    PageResult<SummaryRow> result;
    result.page = query.pagination.page;
    result.pageSize = query.pagination.pageSize;

    auto lease = database();
    if (!lease.isOpen()) {
        return result;
    }

    QElapsedTimer timer;
    timer.start();

    QSqlQuery countQuery(lease.database());
    if (!executeStatement(countQuery,
                          MySqlQueryBuilder::buildSummaryCountStatement(query),
                          "Repository.fetchSummaryRows.count")) {
        return result;
    }
    if (countQuery.next()) {
        result.total = countQuery.value(0).toInt();
    }

    QSqlQuery rowsQuery(lease.database());
    if (!executeStatement(rowsQuery,
                          MySqlQueryBuilder::buildSummaryRowsStatement(query),
                          "Repository.fetchSummaryRows.rows")) {
        return result;
    }

    while (rowsQuery.next()) {
        SummaryRow row;
        row.lineName = rowsQuery.value("line_name").toString();
        row.programName = rowsQuery.value("program_name").toString();
        row.deviceName = rowsQuery.value("device_name").toString();
        row.totalBoards = rowsQuery.value("total_boards").toInt();
        row.goodBoards = rowsQuery.value("good_boards").toInt();
        row.badBoards = rowsQuery.value("bad_boards").toInt();
        row.yieldRate = rowsQuery.value("yield_rate").toDouble();
        row.lastUpdated = rowsQuery.value("last_updated").toDateTime();
        result.rows.append(row);
    }

    Logger::perf("Repository.fetchSummaryRows",
                 timer.elapsed(),
                 QString("rows=%1 total=%2 page=%3 size=%4")
                     .arg(result.rows.size())
                     .arg(result.total)
                     .arg(result.page)
                     .arg(result.pageSize));
    return result;
}

int MySqlSpcRepository::fetchBadPointTotal(const BadStatQuery& query) const {
    m_lastError.clear();
    auto lease = database();
    if (!lease.isOpen()) {
        return 0;
    }

    QSqlQuery sql(lease.database());
    if (!executeStatement(sql,
                          MySqlQueryBuilder::buildBadPointTotalStatement(query),
                          "Repository.fetchBadPointTotal")) {
        return 0;
    }
    return sql.next() ? sql.value(0).toInt() : 0;
}

QList<BadPointStatRow> MySqlSpcRepository::fetchBadPointStats(const BadStatQuery& query) const {
    m_lastError.clear();
    QList<BadPointStatRow> rows;
    auto lease = database();
    if (!lease.isOpen()) {
        return rows;
    }

    QElapsedTimer timer;
    timer.start();

    int total = 0;
    QSqlQuery totalQuery(lease.database());
    if (executeStatement(totalQuery,
                         MySqlQueryBuilder::buildBadPointTotalStatement(query),
                         "Repository.fetchBadPointStats.total") &&
        totalQuery.next()) {
        total = totalQuery.value(0).toInt();
    }

    QSqlQuery sql(lease.database());
    if (!executeStatement(sql,
                          MySqlQueryBuilder::buildBadPointStatsStatement(query),
                          "Repository.fetchBadPointStats.rows")) {
        return rows;
    }

    while (sql.next()) {
        BadPointStatRow row;
        row.badPointName = sql.value("point_name").toString();
        row.count = sql.value("total_count").toInt();
        row.ratio = total == 0 ? 0.0 : static_cast<double>(row.count) * 100.0 / total;
        rows.append(row);
    }

    Logger::perf("Repository.fetchBadPointStats",
                 timer.elapsed(),
                 QString("rows=%1 totalNg=%2 topN=%3").arg(rows.size()).arg(total).arg(query.topN));
    return rows;
}

int MySqlSpcRepository::fetchGradeTotal(const BadStatQuery& query) const {
    m_lastError.clear();
    auto lease = database();
    if (!lease.isOpen()) {
        return 0;
    }

    QSqlQuery sql(lease.database());
    if (!executeStatement(sql,
                          MySqlQueryBuilder::buildGradeTotalStatement(query),
                          "Repository.fetchGradeTotal")) {
        return 0;
    }
    return sql.next() ? sql.value(0).toInt() : 0;
}

QList<GradeStatRow> MySqlSpcRepository::fetchGradeStats(const BadStatQuery& query) const {
    m_lastError.clear();
    QList<GradeStatRow> rows;
    auto lease = database();
    if (!lease.isOpen()) {
        return rows;
    }

    QElapsedTimer timer;
    timer.start();

    int total = 0;
    QSqlQuery totalQuery(lease.database());
    if (executeStatement(totalQuery,
                         MySqlQueryBuilder::buildGradeTotalStatement(query),
                         "Repository.fetchGradeStats.total") &&
        totalQuery.next()) {
        total = totalQuery.value(0).toInt();
    }

    QSqlQuery sql(lease.database());
    if (!executeStatement(sql,
                          MySqlQueryBuilder::buildGradeStatsStatement(query),
                          "Repository.fetchGradeStats.rows")) {
        return rows;
    }

    while (sql.next()) {
        GradeStatRow row;
        row.grade = sql.value("read_grade").toString();
        row.count = sql.value("total_count").toInt();
        row.ratio = total == 0 ? 0.0 : static_cast<double>(row.count) * 100.0 / total;
        rows.append(row);
    }

    Logger::perf("Repository.fetchGradeStats",
                 timer.elapsed(),
                 QString("rows=%1 total=%2").arg(rows.size()).arg(total));
    return rows;
}

PageResult<BoardRecordRow> MySqlSpcRepository::fetchBoardRecords(const BoardRecordQuery& query) const {
    m_lastError.clear();
    PageResult<BoardRecordRow> result;
    result.page = query.pagination.page;
    result.pageSize = query.pagination.pageSize;

    auto lease = database();
    if (!lease.isOpen()) {
        return result;
    }

    QElapsedTimer timer;
    timer.start();

    QSqlQuery countQuery(lease.database());
    if (!executeStatement(countQuery,
                          MySqlQueryBuilder::buildBoardCountStatement(query),
                          "Repository.fetchBoardRecords.count")) {
        return result;
    }
    if (countQuery.next()) {
        result.total = countQuery.value(0).toInt();
    }

    QSqlQuery sql(lease.database());
    if (!executeStatement(sql,
                          MySqlQueryBuilder::buildBoardRowsStatement(query),
                          "Repository.fetchBoardRecords.rows")) {
        return result;
    }

    while (sql.next()) {
        BoardRecordRow row;
        row.boardCode = sql.value("board_code").toString();
        row.result = sql.value("result").toString();
        row.lineName = sql.value("line_name").toString();
        row.programName = sql.value("program_name").toString();
        row.deviceName = sql.value("device_name").toString();
        row.operatorName = sql.value("operator_name").toString();
        row.eventTime = sql.value("event_time").toDateTime();
        result.rows.append(row);
    }

    Logger::perf("Repository.fetchBoardRecords",
                 timer.elapsed(),
                 QString("rows=%1 total=%2 page=%3 size=%4")
                     .arg(result.rows.size())
                     .arg(result.total)
                     .arg(result.page)
                     .arg(result.pageSize));
    return result;
}

PageResult<PointRecordRow> MySqlSpcRepository::fetchPointRecords(const PointRecordQuery& query) const {
    m_lastError.clear();
    PageResult<PointRecordRow> result;
    result.page = query.pagination.page;
    result.pageSize = query.pagination.pageSize;

    auto lease = database();
    if (!lease.isOpen()) {
        return result;
    }

    QElapsedTimer timer;
    timer.start();

    QSqlQuery countQuery(lease.database());
    if (!executeStatement(countQuery,
                          MySqlQueryBuilder::buildPointCountStatement(query),
                          "Repository.fetchPointRecords.count")) {
        return result;
    }
    if (countQuery.next()) {
        result.total = countQuery.value(0).toInt();
    }

    QSqlQuery sql(lease.database());
    if (!executeStatement(sql,
                          MySqlQueryBuilder::buildPointRowsStatement(query),
                          "Repository.fetchPointRecords.rows")) {
        return result;
    }

    while (sql.next()) {
        PointRecordRow row;
        row.boardCode = sql.value("board_code").toString();
        row.pointName = sql.value("point_name").toString();
        row.result = sql.value("result").toString();
        row.readGrade = sql.value("read_grade").toString();
        row.laserContent = sql.value("laser_content").toString();
        row.readCodeContent = sql.value("read_code_content").toString();
        row.isLaser = sql.value("is_laser").toBool();
        row.isReadCode = sql.value("is_read_code").toBool();
        row.lineName = sql.value("line_name").toString();
        row.programName = sql.value("program_name").toString();
        row.startTime = sql.value("start_time").toDateTime();
        row.endTime = sql.value("end_time").toDateTime();
        row.deviceName = sql.value("device_name").toString();
        row.detailJsonPath = sql.value("detail_json_path").toString();
        result.rows.append(row);
    }

    Logger::perf("Repository.fetchPointRecords",
                 timer.elapsed(),
                 QString("rows=%1 total=%2 page=%3 size=%4")
                     .arg(result.rows.size())
                     .arg(result.total)
                     .arg(result.page)
                     .arg(result.pageSize));
    return result;
}

LaserSpc::Domain::LaserContentDuplicateCheckResult MySqlSpcRepository::checkLaserContentDuplicate(
    const QString& laserContent) const {
    m_lastError.clear();
    LaserSpc::Domain::LaserContentDuplicateCheckResult result;
    result.laserContent = laserContent.trimmed();
    if (result.laserContent.isEmpty()) {
        return result;
    }

    auto lease = database();
    if (!lease.isOpen()) {
        return result;
    }

    QSqlQuery sql(lease.database());
    if (!executeStatement(sql,
                          MySqlQueryBuilder::buildLaserContentDuplicateStatement(result.laserContent),
                          "Repository.checkLaserContentDuplicate")) {
        return result;
    }

    if (sql.next()) {
        result.exists = sql.value("duplicate_count").toInt() > 0;
        result.duplicateCount = sql.value("duplicate_count").toInt();
        result.latestBoardCode = sql.value("latest_board_code").toString();
        result.latestPointName = sql.value("latest_point_name").toString();
        result.latestEndTime = sql.value("latest_end_time").toDateTime();
    }
    return result;
}

}  // namespace LaserSpc::Infrastructure
