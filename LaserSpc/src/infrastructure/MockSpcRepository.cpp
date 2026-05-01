#include "infrastructure/MockSpcRepository.h"

#include <algorithm>

#include "infrastructure/PointDetailJsonService.h"

using LaserSpc::Domain::BadPointStatRow;
using LaserSpc::Domain::BadStatQuery;
using LaserSpc::Domain::BoardRecordQuery;
using LaserSpc::Domain::BoardRecordRow;
using LaserSpc::Domain::FilterCriteria;
using LaserSpc::Domain::GradeStatRow;
using LaserSpc::Domain::MetricCardData;
using LaserSpc::Domain::PageResult;
using LaserSpc::Domain::PointDetailInfo;
using LaserSpc::Domain::PointRecordQuery;
using LaserSpc::Domain::PointRecordRow;
using LaserSpc::Domain::SummaryQuery;
using LaserSpc::Domain::SummaryRow;

namespace {

template <typename T>
PageResult<T> paginate(const QList<T>& rows, const LaserSpc::Domain::Pagination& pagination) {
    PageResult<T> result;
    const int rowCount = static_cast<int>(rows.size());
    result.total = rowCount;
    result.page = pagination.page;
    result.pageSize = pagination.pageSize;

    const int begin = std::max(0, (pagination.page - 1) * pagination.pageSize);
    const int end = std::min(rowCount, begin + pagination.pageSize);
    for (int index = begin; index < end; ++index) {
        result.rows.append(rows.at(index));
    }
    return result;
}

bool isAllValue(const QString& value) {
    return value.isEmpty() || value == QObject::tr("全部");
}

QString formatRatio(double value) {
    return QString::number(value, 'f', 2) + "%";
}

}  // namespace

namespace LaserSpc::Infrastructure {

MockSpcRepository::MockSpcRepository() {
    const QDateTime now = QDateTime::currentDateTime();
    const QString detailDirectory = PointDetailJsonService::defaultDetailDirectory();

    m_summaryRows = {
        {"L1", "Program-A", "Laser-01", 1800, 1764, 36, 98.00, now.addSecs(-1200)},
        {"L1", "Program-B", "Laser-02", 1520, 1490, 30, 98.03, now.addSecs(-2400)},
        {"L2", "Program-C", "Laser-03", 1320, 1287, 33, 97.50, now.addSecs(-3600)},
        {"L2", "Program-D", "Laser-04", 1660, 1618, 42, 97.47, now.addSecs(-4800)},
        {"L3", "Program-E", "Laser-05", 920, 901, 19, 97.93, now.addSecs(-5400)},
        {"L3", "Program-F", "Laser-06", 1130, 1099, 31, 97.26, now.addSecs(-6000)},
        {"L4", "Program-G", "Laser-07", 1440, 1405, 35, 97.57, now.addSecs(-6600)},
        {"L5", "Program-H", "Laser-08", 1260, 1224, 36, 97.14, now.addSecs(-7200)},
        {"L6", "Program-I", "Laser-09", 980, 954, 26, 97.35, now.addSecs(-7800)}
    };

    m_badPointRows = {
        {"MarkOffset", 28, 34.57},
        {"CodeBlur", 19, 23.46},
        {"ContrastLow", 15, 18.52},
        {"CellMiss", 11, 13.58},
        {"PrintShift", 8, 9.88},
        {"ReflectNoise", 7, 8.64},
        {"FocusDrift", 6, 7.35}
    };

    m_gradeRows = {
        {"A", 4210, 48.16},
        {"B", 2488, 28.46},
        {"C", 1103, 12.62},
        {"D", 464, 5.31},
        {"E", 291, 3.33},
        {"F", 188, 2.15}
    };

    m_boardRows = {
        {"BD-240301-0014", "OK", "L7", "Program-J", "Laser-10", "Jack", now.addSecs(-430)},
        {"BD-240301-0013", "NG", "L6", "Program-I", "Laser-09", "Ivy", now.addSecs(-450)},
        {"BD-240301-0012", "OK", "L5", "Program-H", "Laser-08", "Helen", now.addSecs(-460)},
        {"BD-240301-0011", "OK", "L4", "Program-G", "Laser-07", "Grace", now.addSecs(-470)},
        {"BD-240301-0010", "NG", "L3", "Program-E", "Laser-05", "Eric", now.addSecs(-480)},
        {"BD-240301-0009", "OK", "L2", "Program-C", "Laser-03", "Chris", now.addSecs(-510)},
        {"BD-240301-0008", "NG", "L1", "Program-B", "Laser-02", "Bob", now.addSecs(-550)},
        {"BD-240301-0007", "OK", "L3", "Program-F", "Laser-06", "Fiona", now.addSecs(-590)},
        {"BD-240301-0006", "OK", "L3", "Program-E", "Laser-05", "Eric", now.addSecs(-600)},
        {"BD-240301-0005", "NG", "L2", "Program-D", "Laser-04", "Diana", now.addSecs(-620)},
        {"BD-240301-0004", "OK", "L2", "Program-C", "Laser-03", "Chris", now.addSecs(-650)},
        {"BD-240301-0003", "NG", "L1", "Program-B", "Laser-02", "Alice", now.addSecs(-680)},
        {"BD-240301-0002", "OK", "L1", "Program-A", "Laser-01", "Bob", now.addSecs(-700)},
        {"BD-240301-0001", "OK", "L1", "Program-A", "Laser-01", "Alice", now.addSecs(-800)}
    };

    auto buildPoint = [&](const QString& boardCode,
                          const QString& pointName,
                          const QString& result,
                          const QString& readGrade,
                          const QString& readCodeContent,
                          bool isLaser,
                          bool isReadCode,
                          const QString& lineName,
                          const QString& programName,
                          const QDateTime& startTime,
                          const QDateTime& endTime,
                          const QString& deviceName) {
        PointRecordRow row;
        row.boardCode = boardCode;
        row.pointName = pointName;
        row.result = result;
        row.readGrade = readGrade;
        row.laserContent = pointName + "-LASER";
        row.readCodeContent = readCodeContent;
        row.isLaser = isLaser;
        row.isReadCode = isReadCode;
        row.lineName = lineName;
        row.programName = programName;
        row.startTime = startTime;
        row.endTime = endTime;
        row.deviceName = deviceName;
        row.detailJsonPath = PointDetailJsonService::buildDefaultFilePath(row, detailDirectory);

        PointDetailInfo detail;
        detail.laserTemplatePath = QString("D:/templates/%1.tpl").arg(programName);
        detail.laserContent = pointName + "-LASER";
        detail.readCodeContent = readCodeContent;
        detail.success = (result == "OK");
        detail.programName = programName;
        detail.startTime = startTime;
        detail.endTime = endTime;

        QString errorMessage;
        PointDetailJsonService::saveDetail(detail, row.detailJsonPath, &errorMessage);
        return row;
    };

    m_pointRows = {
        buildPoint("BD-240301-0014", "Code-J1", "OK", "B", "READ-J1", true, true, "L7", "Program-J", now.addSecs(-470), now.addSecs(-430), "Laser-10"),
        buildPoint("BD-240301-0013", "Code-I1", "NG", "E", "READ-I1", true, true, "L6", "Program-I", now.addSecs(-500), now.addSecs(-450), "Laser-09"),
        buildPoint("BD-240301-0012", "Code-H1", "OK", "A", "READ-H1", true, true, "L5", "Program-H", now.addSecs(-510), now.addSecs(-460), "Laser-08"),
        buildPoint("BD-240301-0011", "Code-G1", "OK", "B", "READ-G1", true, true, "L4", "Program-G", now.addSecs(-520), now.addSecs(-470), "Laser-07"),
        buildPoint("BD-240301-0010", "Code-E2", "NG", "D", "READ-E2", true, true, "L3", "Program-E", now.addSecs(-560), now.addSecs(-480), "Laser-05"),
        buildPoint("BD-240301-0009", "Code-C2", "OK", "A", "READ-C2", true, true, "L2", "Program-C", now.addSecs(-600), now.addSecs(-510), "Laser-03"),
        buildPoint("BD-240301-0008", "Code-B2", "NG", "C", "READ-B2", true, true, "L1", "Program-B", now.addSecs(-610), now.addSecs(-550), "Laser-02"),
        buildPoint("BD-240301-0007", "Code-F1", "OK", "B", "READ-F1", true, true, "L3", "Program-F", now.addSecs(-640), now.addSecs(-590), "Laser-06"),
        buildPoint("BD-240301-0006", "Code-E1", "OK", "A", "READ-E1", true, true, "L3", "Program-E", now.addSecs(-650), now.addSecs(-600), "Laser-05"),
        buildPoint("BD-240301-0005", "Code-D1", "NG", "D", "READ-D1", true, true, "L2", "Program-D", now.addSecs(-670), now.addSecs(-620), "Laser-04"),
        buildPoint("BD-240301-0004", "Code-C1", "OK", "B", "READ-C1", true, true, "L2", "Program-C", now.addSecs(-700), now.addSecs(-650), "Laser-03"),
        buildPoint("BD-240301-0003", "Code-B1", "NG", "C", "READ-B1", true, true, "L1", "Program-B", now.addSecs(-730), now.addSecs(-680), "Laser-02"),
        buildPoint("BD-240301-0002", "Code-A2", "OK", "A", "READ-A2", true, true, "L1", "Program-A", now.addSecs(-760), now.addSecs(-700), "Laser-01"),
        buildPoint("BD-240301-0001", "Code-A1", "OK", "A", "READ-A1", true, true, "L1", "Program-A", now.addSecs(-850), now.addSecs(-800), "Laser-01")
    };
}

QString MockSpcRepository::lastError() const {
    return m_lastError;
}

LaserSpc::Domain::FilterOptions MockSpcRepository::fetchFilterOptions() const {
    m_lastError.clear();

    QStringList lineNames;
    QStringList programNames;
    QStringList deviceNames;
    for (const auto& row : m_boardRows) {
        if (!lineNames.contains(row.lineName)) {
            lineNames.append(row.lineName);
        }
        if (!programNames.contains(row.programName)) {
            programNames.append(row.programName);
        }
        if (!deviceNames.contains(row.deviceName)) {
            deviceNames.append(row.deviceName);
        }
    }

    std::sort(lineNames.begin(), lineNames.end());
    std::sort(programNames.begin(), programNames.end());
    std::sort(deviceNames.begin(), deviceNames.end());

    return LaserSpc::Domain::FilterOptions{lineNames, programNames, deviceNames};
}

bool MockSpcRepository::matchesCommonFilter(const FilterCriteria& filter,
                                            const QString& lineName,
                                            const QString& programName,
                                            const QString& deviceName,
                                            const QString& result,
                                            const QDateTime& eventTime,
                                            const QStringList& searchableTexts) const {
    if (filter.beginTime.isValid() && eventTime < filter.beginTime) {
        return false;
    }
    if (filter.endTime.isValid() && eventTime > filter.endTime) {
        return false;
    }
    if (!isAllValue(filter.lineName) && filter.lineName != lineName) {
        return false;
    }
    if (!isAllValue(filter.programName) && filter.programName != programName) {
        return false;
    }
    if (!isAllValue(filter.deviceName) && filter.deviceName != deviceName) {
        return false;
    }
    if (!isAllValue(filter.result) && filter.result != result) {
        return false;
    }
    if (filter.keyword.isEmpty()) {
        return true;
    }

    for (const QString& text : searchableTexts) {
        if (text.contains(filter.keyword, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

int MockSpcRepository::filterWeight(const FilterCriteria& filter) const {
    int weight = 0;
    if (!isAllValue(filter.lineName)) {
        ++weight;
    }
    if (!isAllValue(filter.programName)) {
        ++weight;
    }
    if (!isAllValue(filter.deviceName)) {
        ++weight;
    }
    if (!isAllValue(filter.result)) {
        ++weight;
    }
    if (!filter.keyword.isEmpty()) {
        ++weight;
    }
    return weight;
}

QList<MetricCardData> MockSpcRepository::fetchSummaryMetrics(const SummaryQuery& query) const {
    m_lastError.clear();
    SummaryQuery fullQuery = query;
    fullQuery.pagination.page = 1;
    fullQuery.pagination.pageSize = std::max(1, static_cast<int>(m_summaryRows.size()));
    const auto rows = fetchSummaryRows(fullQuery).rows;
    int totalBoards = 0;
    int goodBoards = 0;
    int badBoards = 0;
    for (const SummaryRow& row : rows) {
        totalBoards += row.totalBoards;
        goodBoards += row.goodBoards;
        badBoards += row.badBoards;
    }
    const double yieldRate = totalBoards == 0 ? 0.0 : static_cast<double>(goodBoards) * 100.0 / totalBoards;

    return {
        {"总板数", QString::number(totalBoards), "当前筛选结果下的板级总量"},
        {"良板数", QString::number(goodBoards), "板级 OK 数量"},
        {"不良板数", QString::number(badBoards), "板级 NG 数量"},
        {"良率", formatRatio(yieldRate), "按良板数 / 总板数计算"}
    };
}

PageResult<SummaryRow> MockSpcRepository::fetchSummaryRows(const SummaryQuery& query) const {
    m_lastError.clear();
    QList<SummaryRow> rows;
    for (const SummaryRow& row : m_summaryRows) {
        if (matchesCommonFilter(query.filter,
                                row.lineName,
                                row.programName,
                                row.deviceName,
                                row.badBoards > 0 ? "NG" : "OK",
                                row.lastUpdated,
                                {row.lineName, row.programName, row.deviceName})) {
            rows.append(row);
        }
    }

    std::sort(rows.begin(), rows.end(), [&](const SummaryRow& left, const SummaryRow& right) {
        if (query.sort.field == "lineName") {
            return query.sort.order == Qt::AscendingOrder ? left.lineName < right.lineName : left.lineName > right.lineName;
        }
        if (query.sort.field == "programName") {
            return query.sort.order == Qt::AscendingOrder ? left.programName < right.programName
                                                          : left.programName > right.programName;
        }
        if (query.sort.field == "deviceName") {
            return query.sort.order == Qt::AscendingOrder ? left.deviceName < right.deviceName
                                                          : left.deviceName > right.deviceName;
        }
        if (query.sort.field == "yieldRate") {
            return query.sort.order == Qt::AscendingOrder ? left.yieldRate < right.yieldRate
                                                          : left.yieldRate > right.yieldRate;
        }
        if (query.sort.field == "totalBoards") {
            return query.sort.order == Qt::AscendingOrder ? left.totalBoards < right.totalBoards
                                                          : left.totalBoards > right.totalBoards;
        }
        if (query.sort.field == "goodBoards") {
            return query.sort.order == Qt::AscendingOrder ? left.goodBoards < right.goodBoards
                                                          : left.goodBoards > right.goodBoards;
        }
        if (query.sort.field == "badBoards") {
            return query.sort.order == Qt::AscendingOrder ? left.badBoards < right.badBoards
                                                          : left.badBoards > right.badBoards;
        }
        return query.sort.order == Qt::AscendingOrder ? left.lastUpdated < right.lastUpdated
                                                      : left.lastUpdated > right.lastUpdated;
    });

    return paginate(rows, query.pagination);
}

int MockSpcRepository::fetchBadPointTotal(const BadStatQuery& query) const {
    int total = 0;
    const int adjustment = filterWeight(query.filter) * 2;
    for (const BadPointStatRow& row : m_badPointRows) {
        total += std::max(1, row.count - adjustment);
    }
    return total;
}

QList<BadPointStatRow> MockSpcRepository::fetchBadPointStats(const BadStatQuery& query) const {
    m_lastError.clear();
    QList<BadPointStatRow> rows;
    const int adjustment = filterWeight(query.filter) * 2;
    int total = 0;

    for (const BadPointStatRow& row : m_badPointRows) {
        BadPointStatRow item = row;
        item.count = std::max(1, item.count - adjustment);
        total += item.count;
        rows.append(item);
    }

    for (BadPointStatRow& row : rows) {
        row.ratio = total == 0 ? 0.0 : static_cast<double>(row.count) * 100.0 / total;
    }

    if (query.topN > 0 && rows.size() > query.topN) {
        rows = rows.mid(0, query.topN);
    }
    return rows;
}

int MockSpcRepository::fetchGradeTotal(const BadStatQuery& query) const {
    int total = 0;
    const int adjustment = filterWeight(query.filter) * 120;
    for (const GradeStatRow& row : m_gradeRows) {
        total += std::max(1, row.count - adjustment);
    }
    return total;
}

QList<GradeStatRow> MockSpcRepository::fetchGradeStats(const BadStatQuery& query) const {
    m_lastError.clear();
    QList<GradeStatRow> rows;
    const int adjustment = filterWeight(query.filter) * 120;
    int total = 0;

    for (const GradeStatRow& row : m_gradeRows) {
        GradeStatRow item = row;
        item.count = std::max(1, item.count - adjustment);
        total += item.count;
        rows.append(item);
    }

    for (GradeStatRow& row : rows) {
        row.ratio = total == 0 ? 0.0 : static_cast<double>(row.count) * 100.0 / total;
    }
    return rows;
}

PageResult<BoardRecordRow> MockSpcRepository::fetchBoardRecords(const BoardRecordQuery& query) const {
    m_lastError.clear();
    QList<BoardRecordRow> rows;
    for (const BoardRecordRow& row : m_boardRows) {
        if (matchesCommonFilter(query.filter,
                                row.lineName,
                                row.programName,
                                row.deviceName,
                                row.result,
                                row.eventTime,
                                {row.boardCode, row.operatorName, row.programName})) {
            rows.append(row);
        }
    }

    std::sort(rows.begin(), rows.end(), [&](const BoardRecordRow& left, const BoardRecordRow& right) {
        if (query.sort.field == "boardCode") {
            return query.sort.order == Qt::AscendingOrder ? left.boardCode < right.boardCode
                                                          : left.boardCode > right.boardCode;
        }
        if (query.sort.field == "result") {
            return query.sort.order == Qt::AscendingOrder ? left.result < right.result : left.result > right.result;
        }
        if (query.sort.field == "lineName") {
            return query.sort.order == Qt::AscendingOrder ? left.lineName < right.lineName
                                                          : left.lineName > right.lineName;
        }
        if (query.sort.field == "programName") {
            return query.sort.order == Qt::AscendingOrder ? left.programName < right.programName
                                                          : left.programName > right.programName;
        }
        if (query.sort.field == "deviceName") {
            return query.sort.order == Qt::AscendingOrder ? left.deviceName < right.deviceName
                                                          : left.deviceName > right.deviceName;
        }
        if (query.sort.field == "operatorName") {
            return query.sort.order == Qt::AscendingOrder ? left.operatorName < right.operatorName
                                                          : left.operatorName > right.operatorName;
        }
        return query.sort.order == Qt::AscendingOrder ? left.eventTime < right.eventTime
                                                      : left.eventTime > right.eventTime;
    });

    return paginate(rows, query.pagination);
}

PageResult<PointRecordRow> MockSpcRepository::fetchPointRecords(const PointRecordQuery& query) const {
    m_lastError.clear();
    QList<PointRecordRow> rows;
    for (const PointRecordRow& row : m_pointRows) {
        if (matchesCommonFilter(query.filter,
                                row.lineName,
                                row.programName,
                                row.deviceName,
                                row.result,
                                row.endTime,
                                {row.boardCode,
                                 row.pointName,
                                 row.readGrade,
                                 row.laserContent,
                                 row.readCodeContent,
                                 row.lineName,
                                 row.programName,
                                 row.isLaser ? QObject::tr("是") : QObject::tr("否"),
                                 row.isReadCode ? QObject::tr("是") : QObject::tr("否")})) {
            rows.append(row);
        }
    }

    std::sort(rows.begin(), rows.end(), [&](const PointRecordRow& left, const PointRecordRow& right) {
        if (query.sort.field == "boardCode") {
            return query.sort.order == Qt::AscendingOrder ? left.boardCode < right.boardCode
                                                          : left.boardCode > right.boardCode;
        }
        if (query.sort.field == "pointName") {
            return query.sort.order == Qt::AscendingOrder ? left.pointName < right.pointName
                                                          : left.pointName > right.pointName;
        }
        if (query.sort.field == "result") {
            return query.sort.order == Qt::AscendingOrder ? left.result < right.result : left.result > right.result;
        }
        if (query.sort.field == "readGrade") {
            return query.sort.order == Qt::AscendingOrder ? left.readGrade < right.readGrade
                                                          : left.readGrade > right.readGrade;
        }
        if (query.sort.field == "readCodeContent") {
            return query.sort.order == Qt::AscendingOrder ? left.readCodeContent < right.readCodeContent
                                                          : left.readCodeContent > right.readCodeContent;
        }
        if (query.sort.field == "laserContent") {
            return query.sort.order == Qt::AscendingOrder ? left.laserContent < right.laserContent
                                                          : left.laserContent > right.laserContent;
        }
        if (query.sort.field == "isLaser") {
            return query.sort.order == Qt::AscendingOrder ? left.isLaser < right.isLaser
                                                          : left.isLaser > right.isLaser;
        }
        if (query.sort.field == "isReadCode") {
            return query.sort.order == Qt::AscendingOrder ? left.isReadCode < right.isReadCode
                                                          : left.isReadCode > right.isReadCode;
        }
        if (query.sort.field == "lineName") {
            return query.sort.order == Qt::AscendingOrder ? left.lineName < right.lineName
                                                          : left.lineName > right.lineName;
        }
        if (query.sort.field == "programName") {
            return query.sort.order == Qt::AscendingOrder ? left.programName < right.programName
                                                          : left.programName > right.programName;
        }
        if (query.sort.field == "startTime") {
            return query.sort.order == Qt::AscendingOrder ? left.startTime < right.startTime
                                                          : left.startTime > right.startTime;
        }
        if (query.sort.field == "deviceName") {
            return query.sort.order == Qt::AscendingOrder ? left.deviceName < right.deviceName
                                                          : left.deviceName > right.deviceName;
        }
        if (query.sort.field == "detailJsonPath") {
            return query.sort.order == Qt::AscendingOrder ? left.detailJsonPath < right.detailJsonPath
                                                          : left.detailJsonPath > right.detailJsonPath;
        }
        return query.sort.order == Qt::AscendingOrder ? left.endTime < right.endTime
                                                      : left.endTime > right.endTime;
    });

    return paginate(rows, query.pagination);
}

LaserSpc::Domain::LaserContentDuplicateCheckResult MockSpcRepository::checkLaserContentDuplicate(
    const QString& laserContent) const {
    m_lastError.clear();
    LaserSpc::Domain::LaserContentDuplicateCheckResult result;
    result.laserContent = laserContent.trimmed();
    if (result.laserContent.isEmpty()) {
        return result;
    }

    for (const auto& row : m_pointRows) {
        if (row.laserContent != result.laserContent) {
            continue;
        }
        result.exists = true;
        ++result.duplicateCount;
        if (!result.latestEndTime.isValid() || row.endTime > result.latestEndTime) {
            result.latestBoardCode = row.boardCode;
            result.latestPointName = row.pointName;
            result.latestEndTime = row.endTime;
        }
    }
    return result;
}

}  // namespace LaserSpc::Infrastructure
