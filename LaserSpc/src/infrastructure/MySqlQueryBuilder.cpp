#include "infrastructure/MySqlQueryBuilder.h"

#include "ui/common/UiTextCatalog.h"

namespace {

QString allSelectionValue() {
    return LaserSpc::Ui::TextCatalog::allSelection();
}

bool isAllSelection(const QString& value) {
    return value.isEmpty() || value == allSelectionValue();
}

QString toSqlDateTime(const QDateTime& value) {
    return value.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

}  // namespace

namespace LaserSpc::Infrastructure {

QString MySqlQueryBuilder::formatPercent(double value) {
    return QString::number(value, 'f', 2) + "%";
}

QString MySqlQueryBuilder::filterOptionColumn(const QString& field) {
    if (field == "line_name") {
        return "line_name";
    }
    if (field == "program_name") {
        return "program_name";
    }
    if (field == "device_name") {
        return "device_name";
    }
    return QString();
}

QString MySqlQueryBuilder::summaryOrderField(const QString& field) {
    if (field == "lineName") {
        return "line_name";
    }
    if (field == "programName") {
        return "program_name";
    }
    if (field == "deviceName") {
        return "device_name";
    }
    if (field == "yieldRate") {
        return "yield_rate";
    }
    if (field == "totalBoards") {
        return "total_boards";
    }
    if (field == "goodBoards") {
        return "good_boards";
    }
    if (field == "badBoards") {
        return "bad_boards";
    }
    return "last_updated";
}

QString MySqlQueryBuilder::boardOrderField(const QString& field) {
    if (field == "boardCode") {
        return "board_code";
    }
    if (field == "result") {
        return "result";
    }
    if (field == "lineName") {
        return "line_name";
    }
    if (field == "programName") {
        return "program_name";
    }
    if (field == "deviceName") {
        return "device_name";
    }
    if (field == "operatorName") {
        return "operator_name";
    }
    if (field == "eventTime") {
        return "event_time";
    }
    return "event_time";
}

QString MySqlQueryBuilder::pointOrderField(const QString& field) {
    if (field == "boardCode") {
        return "board_code";
    }
    if (field == "pointName") {
        return "point_name";
    }
    if (field == "result") {
        return "result";
    }
    if (field == "readGrade") {
        return "read_grade";
    }
    if (field == "readCodeContent") {
        return "read_code_content";
    }
    if (field == "laserContent") {
        return "laser_content";
    }
    if (field == "isLaser") {
        return "is_laser";
    }
    if (field == "isReadCode") {
        return "is_read_code";
    }
    if (field == "lineName") {
        return "line_name";
    }
    if (field == "programName") {
        return "program_name";
    }
    if (field == "startTime") {
        return "start_time";
    }
    if (field == "deviceName") {
        return "device_name";
    }
    if (field == "endTime") {
        return "end_time";
    }
    if (field == "detailJsonPath") {
        return "detail_json_path";
    }
    return "end_time";
}

QString MySqlQueryBuilder::sortDirection(Qt::SortOrder order) {
    return order == Qt::AscendingOrder ? "ASC" : "DESC";
}

MySqlWhereClause MySqlQueryBuilder::buildBoardWhere(const LaserSpc::Domain::FilterCriteria& filter) {
    MySqlWhereClause where;
    QStringList clauses;

    if (filter.beginTime.isValid()) {
        clauses << "event_time >= ?";
        where.values.append(toSqlDateTime(filter.beginTime));
    }
    if (filter.endTime.isValid()) {
        clauses << "event_time <= ?";
        where.values.append(toSqlDateTime(filter.endTime));
    }
    if (!isAllSelection(filter.lineName)) {
        clauses << "line_name = ?";
        where.values.append(filter.lineName);
    }
    if (!isAllSelection(filter.programName)) {
        clauses << "program_name = ?";
        where.values.append(filter.programName);
    }
    if (!isAllSelection(filter.deviceName)) {
        clauses << "device_name = ?";
        where.values.append(filter.deviceName);
    }
    if (!isAllSelection(filter.result)) {
        clauses << "result = ?";
        where.values.append(filter.result);
    }
    if (!filter.keyword.isEmpty()) {
        clauses << "(board_code LIKE ? OR operator_name LIKE ? OR program_name LIKE ?)";
        const QString keyword = "%" + filter.keyword + "%";
        where.values.append(keyword);
        where.values.append(keyword);
        where.values.append(keyword);
    }

    where.clause = clauses.isEmpty() ? QString() : " WHERE " + clauses.join(" AND ");
    return where;
}

MySqlWhereClause MySqlQueryBuilder::buildPointWhere(const LaserSpc::Domain::FilterCriteria& filter) {
    MySqlWhereClause where;
    QStringList clauses;

    if (filter.beginTime.isValid()) {
        clauses << "end_time >= ?";
        where.values.append(toSqlDateTime(filter.beginTime));
    }
    if (filter.endTime.isValid()) {
        clauses << "end_time <= ?";
        where.values.append(toSqlDateTime(filter.endTime));
    }
    if (!isAllSelection(filter.lineName)) {
        clauses << "line_name = ?";
        where.values.append(filter.lineName);
    }
    if (!isAllSelection(filter.programName)) {
        clauses << "program_name = ?";
        where.values.append(filter.programName);
    }
    if (!isAllSelection(filter.deviceName)) {
        clauses << "device_name = ?";
        where.values.append(filter.deviceName);
    }
    if (!isAllSelection(filter.result)) {
        clauses << "result = ?";
        where.values.append(filter.result);
    }
    if (!filter.keyword.isEmpty()) {
        clauses << "(board_code LIKE ? OR point_name LIKE ? OR read_grade LIKE ? OR laser_content LIKE ? OR read_code_content LIKE ?)";
        const QString keyword = "%" + filter.keyword + "%";
        where.values.append(keyword);
        where.values.append(keyword);
        where.values.append(keyword);
        where.values.append(keyword);
        where.values.append(keyword);
    }

    where.clause = clauses.isEmpty() ? QString() : " WHERE " + clauses.join(" AND ");
    return where;
}

MySqlSqlStatement MySqlQueryBuilder::buildFilterOptionsStatement(const QString& columnName) {
    const QString normalizedColumn = filterOptionColumn(columnName);
    if (normalizedColumn.isEmpty()) {
        return {"SELECT NULL WHERE 1 = 0", {}};
    }

    return {
        "SELECT DISTINCT " + normalizedColumn + " FROM board_records "
        "WHERE " + normalizedColumn + " IS NOT NULL AND " + normalizedColumn + " <> '' "
        "ORDER BY " + normalizedColumn + " ASC",
        {}
    };
}

MySqlSqlStatement MySqlQueryBuilder::buildSummaryMetricsStatement(const LaserSpc::Domain::SummaryQuery& query) {
    const auto where = buildBoardWhere(query.filter);
    return {
        "SELECT COUNT(*) AS total_boards, "
        "SUM(CASE WHEN result = 'OK' THEN 1 ELSE 0 END) AS good_boards, "
        "SUM(CASE WHEN result = 'NG' THEN 1 ELSE 0 END) AS bad_boards "
        "FROM board_records" + where.clause,
        where.values
    };
}

MySqlSqlStatement MySqlQueryBuilder::buildSummaryCountStatement(const LaserSpc::Domain::SummaryQuery& query) {
    const auto where = buildBoardWhere(query.filter);
    return {
        "SELECT COUNT(*) FROM ("
        "SELECT line_name, program_name, device_name "
        "FROM board_records" + where.clause + " "
        "GROUP BY line_name, program_name, device_name"
        ") AS grouped_rows",
        where.values
    };
}

MySqlSqlStatement MySqlQueryBuilder::buildSummaryRowsStatement(const LaserSpc::Domain::SummaryQuery& query) {
    const auto where = buildBoardWhere(query.filter);
    const int offset = (query.pagination.page - 1) * query.pagination.pageSize;
    MySqlSqlStatement statement{
        "SELECT line_name, program_name, device_name, "
        "COUNT(*) AS total_boards, "
        "SUM(CASE WHEN result = 'OK' THEN 1 ELSE 0 END) AS good_boards, "
        "SUM(CASE WHEN result = 'NG' THEN 1 ELSE 0 END) AS bad_boards, "
        "MAX(event_time) AS last_updated, "
        "ROUND(SUM(CASE WHEN result = 'OK' THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS yield_rate "
        "FROM board_records" + where.clause + " "
        "GROUP BY line_name, program_name, device_name "
        "ORDER BY " + summaryOrderField(query.sort.field) + " " + sortDirection(query.sort.order) + " "
        "LIMIT ? OFFSET ?",
        where.values
    };
    statement.values.append(query.pagination.pageSize);
    statement.values.append(offset);
    return statement;
}

MySqlSqlStatement MySqlQueryBuilder::buildBadPointTotalStatement(const LaserSpc::Domain::BadStatQuery& query) {
    auto where = buildPointWhere(query.filter);
    if (where.clause.isEmpty()) {
        where.clause = " WHERE result = 'NG'";
    } else {
        where.clause += " AND result = 'NG'";
    }
    return {
        "SELECT COUNT(*) FROM point_records" + where.clause,
        where.values
    };
}

MySqlSqlStatement MySqlQueryBuilder::buildBadPointStatsStatement(const LaserSpc::Domain::BadStatQuery& query) {
    auto where = buildPointWhere(query.filter);
    if (where.clause.isEmpty()) {
        where.clause = " WHERE result = 'NG'";
    } else {
        where.clause += " AND result = 'NG'";
    }
    MySqlSqlStatement statement{
        "SELECT point_name, COUNT(*) AS total_count "
        "FROM point_records" + where.clause + " "
        "GROUP BY point_name "
        "ORDER BY total_count DESC, point_name ASC "
        "LIMIT ?",
        where.values
    };
    statement.values.append(query.topN);
    return statement;
}

MySqlSqlStatement MySqlQueryBuilder::buildGradeTotalStatement(const LaserSpc::Domain::BadStatQuery& query) {
    const auto where = buildPointWhere(query.filter);
    return {
        "SELECT COUNT(*) FROM point_records" + where.clause,
        where.values
    };
}

MySqlSqlStatement MySqlQueryBuilder::buildGradeStatsStatement(const LaserSpc::Domain::BadStatQuery& query) {
    const auto where = buildPointWhere(query.filter);
    return {
        "SELECT read_grade, COUNT(*) AS total_count "
        "FROM point_records" + where.clause + " "
        "GROUP BY read_grade "
        "ORDER BY total_count DESC, read_grade ASC",
        where.values
    };
}

MySqlSqlStatement MySqlQueryBuilder::buildBoardCountStatement(const LaserSpc::Domain::BoardRecordQuery& query) {
    const auto where = buildBoardWhere(query.filter);
    return {
        "SELECT COUNT(*) FROM board_records" + where.clause,
        where.values
    };
}

MySqlSqlStatement MySqlQueryBuilder::buildBoardRowsStatement(const LaserSpc::Domain::BoardRecordQuery& query) {
    const auto where = buildBoardWhere(query.filter);
    const int offset = (query.pagination.page - 1) * query.pagination.pageSize;
    MySqlSqlStatement statement{
        "SELECT board_code, result, line_name, program_name, device_name, operator_name, event_time "
        "FROM board_records" + where.clause + " "
        "ORDER BY " + boardOrderField(query.sort.field) + " " + sortDirection(query.sort.order) + " "
        "LIMIT ? OFFSET ?",
        where.values
    };
    statement.values.append(query.pagination.pageSize);
    statement.values.append(offset);
    return statement;
}

MySqlSqlStatement MySqlQueryBuilder::buildPointCountStatement(const LaserSpc::Domain::PointRecordQuery& query) {
    const auto where = buildPointWhere(query.filter);
    return {
        "SELECT COUNT(*) FROM point_records" + where.clause,
        where.values
    };
}

MySqlSqlStatement MySqlQueryBuilder::buildPointRowsStatement(const LaserSpc::Domain::PointRecordQuery& query) {
    const auto where = buildPointWhere(query.filter);
    const int offset = (query.pagination.page - 1) * query.pagination.pageSize;
    MySqlSqlStatement statement{
        "SELECT board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code, line_name, program_name, start_time, end_time, device_name, detail_json_path "
        "FROM point_records" + where.clause + " "
        "ORDER BY " + pointOrderField(query.sort.field) + " " + sortDirection(query.sort.order) + " "
        "LIMIT ? OFFSET ?",
        where.values
    };
    statement.values.append(query.pagination.pageSize);
    statement.values.append(offset);
    return statement;
}

MySqlSqlStatement MySqlQueryBuilder::buildLaserContentDuplicateStatement(const QString& laserContent) {
    return {
        "SELECT laser_content, COUNT(*) AS duplicate_count, "
        "SUBSTRING_INDEX(GROUP_CONCAT(board_code ORDER BY end_time DESC SEPARATOR ','), ',', 1) AS latest_board_code, "
        "SUBSTRING_INDEX(GROUP_CONCAT(point_name ORDER BY end_time DESC SEPARATOR ','), ',', 1) AS latest_point_name, "
        "MAX(end_time) AS latest_end_time "
        "FROM point_records "
        "WHERE laser_content = ? "
        "GROUP BY laser_content",
        {laserContent}
    };
}

}  // namespace LaserSpc::Infrastructure
