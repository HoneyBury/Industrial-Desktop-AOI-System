#pragma once

#include <QStringList>
#include <QVariant>

#include "domain/Repository.h"
#include "infrastructure/DatabaseConnection.h"
#include "infrastructure/MySqlQueryBuilder.h"

class QSqlQuery;

namespace LaserSpc::Infrastructure {

class MySqlSpcRepository : public LaserSpc::Domain::ISpcQueryRepository {
public:
    explicit MySqlSpcRepository(DatabaseSettings settings);

    bool isAvailable() const;
    QString lastError() const override;
    LaserSpc::Domain::FilterOptions fetchFilterOptions() const override;

    QList<LaserSpc::Domain::MetricCardData> fetchSummaryMetrics(
        const LaserSpc::Domain::SummaryQuery& query) const override;
    LaserSpc::Domain::PageResult<LaserSpc::Domain::SummaryRow> fetchSummaryRows(
        const LaserSpc::Domain::SummaryQuery& query) const override;
    int fetchBadPointTotal(const LaserSpc::Domain::BadStatQuery& query) const override;
    QList<LaserSpc::Domain::BadPointStatRow> fetchBadPointStats(
        const LaserSpc::Domain::BadStatQuery& query) const override;
    int fetchGradeTotal(const LaserSpc::Domain::BadStatQuery& query) const override;
    QList<LaserSpc::Domain::GradeStatRow> fetchGradeStats(
        const LaserSpc::Domain::BadStatQuery& query) const override;
    LaserSpc::Domain::PageResult<LaserSpc::Domain::BoardRecordRow> fetchBoardRecords(
        const LaserSpc::Domain::BoardRecordQuery& query) const override;
    LaserSpc::Domain::PageResult<LaserSpc::Domain::PointRecordRow> fetchPointRecords(
        const LaserSpc::Domain::PointRecordQuery& query) const override;
    LaserSpc::Domain::LaserContentDuplicateCheckResult checkLaserContentDuplicate(
        const QString& laserContent) const override;

private:
    DatabaseConnection::ConnectionLease database() const;
    QStringList queryDistinctValues(const QString& columnName) const;
    bool executeStatement(QSqlQuery& query, const MySqlSqlStatement& statement, const QString& label) const;

    DatabaseSettings m_settings;
    mutable QString m_lastError;
};

}  // namespace LaserSpc::Infrastructure
