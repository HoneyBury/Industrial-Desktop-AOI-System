#pragma once

#include <QStringList>

#include "domain/Repository.h"

namespace LaserSpc::Infrastructure {

class MockSpcRepository : public LaserSpc::Domain::ISpcQueryRepository {
public:
    MockSpcRepository();

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
    bool matchesCommonFilter(const LaserSpc::Domain::FilterCriteria& filter,
                             const QString& lineName,
                             const QString& programName,
                             const QString& deviceName,
                             const QString& result,
                             const QDateTime& eventTime,
                             const QStringList& searchableTexts) const;
    int filterWeight(const LaserSpc::Domain::FilterCriteria& filter) const;

    QList<LaserSpc::Domain::SummaryRow> m_summaryRows;
    QList<LaserSpc::Domain::BadPointStatRow> m_badPointRows;
    QList<LaserSpc::Domain::GradeStatRow> m_gradeRows;
    QList<LaserSpc::Domain::BoardRecordRow> m_boardRows;
    QList<LaserSpc::Domain::PointRecordRow> m_pointRows;
    mutable QString m_lastError;
};

}  // namespace LaserSpc::Infrastructure
