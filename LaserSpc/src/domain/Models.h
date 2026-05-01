#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <Qt>

namespace LaserSpc::Domain {

enum class PageId {
    Summary = 0,
    BadStat,
    BoardRecord,
    PointRecord
};

struct Pagination {
    int page = 1;
    int pageSize = 20;
};

struct SortOption {
    QString field = "eventTime";
    Qt::SortOrder order = Qt::DescendingOrder;
};

struct FilterCriteria {
    QDateTime beginTime;
    QDateTime endTime;
    QString lineName;
    QString programName;
    QString deviceName;
    QString result;
    QString keyword;
};

struct FilterOptions {
    QStringList lineNames;
    QStringList programNames;
    QStringList deviceNames;
};

struct SummaryQuery {
    FilterCriteria filter;
    Pagination pagination;
    SortOption sort;
};

struct BadStatQuery {
    FilterCriteria filter;
    int topN = 10;
};

struct BoardRecordQuery {
    FilterCriteria filter;
    Pagination pagination;
    SortOption sort;
};

struct PointRecordQuery {
    FilterCriteria filter;
    Pagination pagination;
    SortOption sort;
};

struct MetricCardData {
    QString title;
    QString value;
    QString description;
};

struct SummaryRow {
    QString lineName;
    QString programName;
    QString deviceName;
    int totalBoards = 0;
    int goodBoards = 0;
    int badBoards = 0;
    double yieldRate = 0.0;
    QDateTime lastUpdated;
};

struct BadPointStatRow {
    QString badPointName;
    int count = 0;
    double ratio = 0.0;
};

struct LaserContentDuplicateCheckResult {
    QString laserContent;
    bool exists = false;
    int duplicateCount = 0;
    QString latestBoardCode;
    QString latestPointName;
    QDateTime latestEndTime;
};

struct GradeStatRow {
    QString grade;
    int count = 0;
    double ratio = 0.0;
};

struct BoardRecordRow {
    QString boardCode;
    QString result;
    QString lineName;
    QString programName;
    QString deviceName;
    QString operatorName;
    QDateTime eventTime;
};

struct PointDetailInfo {
    QString laserTemplatePath;
    QString laserContent;
    QString readCodeContent;
    bool success = false;
    QString programName;
    QDateTime startTime;
    QDateTime endTime;
    QJsonObject extraFields;
    QJsonObject fieldDisplayNames;
    QJsonArray algorithmPlan;
};

inline bool operator==(const PointDetailInfo& left, const PointDetailInfo& right) {
    return left.laserTemplatePath == right.laserTemplatePath &&
           left.laserContent == right.laserContent &&
           left.readCodeContent == right.readCodeContent &&
           left.success == right.success &&
           left.programName == right.programName &&
           left.startTime == right.startTime &&
           left.endTime == right.endTime &&
           left.extraFields == right.extraFields &&
           left.fieldDisplayNames == right.fieldDisplayNames &&
           left.algorithmPlan == right.algorithmPlan;
}

struct PointRecordRow {
    QString boardCode;
    QString pointName;
    QString result;
    QString readGrade;
    QString laserContent;
    QString readCodeContent;
    bool isLaser = false;
    bool isReadCode = false;
    QString lineName;
    QString programName;
    QDateTime startTime;
    QDateTime endTime;
    QString deviceName;
    QString detailJsonPath;
    PointDetailInfo detail;
};

struct InspectionBatch {
    QString requestId;
    BoardRecordRow board;
    QList<PointRecordRow> points;
};

struct IngestResult {
    bool success = false;
    int insertedBoards = 0;
    int insertedPoints = 0;
    QString errorMessage;
    QString forwardMessage;
};

template <typename T>
struct PageResult {
    QList<T> rows;
    int total = 0;
    int page = 1;
    int pageSize = 20;
};

}  // namespace LaserSpc::Domain
