#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "domain/Models.h"

namespace LaserSpc::Infrastructure {

struct MySqlWhereClause {
    QString clause;
    QList<QVariant> values;
};

struct MySqlSqlStatement {
    QString sql;
    QList<QVariant> values;
};

class MySqlQueryBuilder {
public:
    static QString formatPercent(double value);
    static QString filterOptionColumn(const QString& field);

    static QString summaryOrderField(const QString& field);
    static QString boardOrderField(const QString& field);
    static QString pointOrderField(const QString& field);
    static QString sortDirection(Qt::SortOrder order);

    static MySqlWhereClause buildBoardWhere(const LaserSpc::Domain::FilterCriteria& filter);
    static MySqlWhereClause buildPointWhere(const LaserSpc::Domain::FilterCriteria& filter);

    static MySqlSqlStatement buildFilterOptionsStatement(const QString& columnName);
    static MySqlSqlStatement buildSummaryMetricsStatement(const LaserSpc::Domain::SummaryQuery& query);
    static MySqlSqlStatement buildSummaryCountStatement(const LaserSpc::Domain::SummaryQuery& query);
    static MySqlSqlStatement buildSummaryRowsStatement(const LaserSpc::Domain::SummaryQuery& query);
    static MySqlSqlStatement buildBadPointTotalStatement(const LaserSpc::Domain::BadStatQuery& query);
    static MySqlSqlStatement buildBadPointStatsStatement(const LaserSpc::Domain::BadStatQuery& query);
    static MySqlSqlStatement buildGradeTotalStatement(const LaserSpc::Domain::BadStatQuery& query);
    static MySqlSqlStatement buildGradeStatsStatement(const LaserSpc::Domain::BadStatQuery& query);
    static MySqlSqlStatement buildBoardCountStatement(const LaserSpc::Domain::BoardRecordQuery& query);
    static MySqlSqlStatement buildBoardRowsStatement(const LaserSpc::Domain::BoardRecordQuery& query);
    static MySqlSqlStatement buildPointCountStatement(const LaserSpc::Domain::PointRecordQuery& query);
    static MySqlSqlStatement buildPointRowsStatement(const LaserSpc::Domain::PointRecordQuery& query);
    static MySqlSqlStatement buildLaserContentDuplicateStatement(const QString& laserContent);
};

}  // namespace LaserSpc::Infrastructure
