#pragma once

#include "domain/Models.h"

namespace LaserSpc::Domain {

class ISpcQueryRepository {
public:
    virtual ~ISpcQueryRepository() = default;

    virtual QString lastError() const = 0;

    virtual FilterOptions fetchFilterOptions() const = 0;
    virtual QList<MetricCardData> fetchSummaryMetrics(const SummaryQuery& query) const = 0;
    virtual PageResult<SummaryRow> fetchSummaryRows(const SummaryQuery& query) const = 0;
    virtual int fetchBadPointTotal(const BadStatQuery& query) const = 0;
    virtual QList<BadPointStatRow> fetchBadPointStats(const BadStatQuery& query) const = 0;
    virtual int fetchGradeTotal(const BadStatQuery& query) const = 0;
    virtual QList<GradeStatRow> fetchGradeStats(const BadStatQuery& query) const = 0;
    virtual PageResult<BoardRecordRow> fetchBoardRecords(const BoardRecordQuery& query) const = 0;
    virtual PageResult<PointRecordRow> fetchPointRecords(const PointRecordQuery& query) const = 0;
    virtual LaserContentDuplicateCheckResult checkLaserContentDuplicate(const QString& laserContent) const = 0;
};

}  // namespace LaserSpc::Domain
