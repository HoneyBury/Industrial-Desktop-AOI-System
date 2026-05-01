#include "app/QueryGateways.h"

namespace LaserSpc::App {

RepositorySummaryQueryGateway::RepositorySummaryQueryGateway(
    std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> repository)
    : m_repository(std::move(repository)) {}

SummaryPageData RepositorySummaryQueryGateway::querySummary(const LaserSpc::Domain::SummaryQuery& query) const {
    return SummaryPageData{m_repository->fetchSummaryMetrics(query), m_repository->fetchSummaryRows(query)};
}

LaserSpc::Domain::FilterOptions RepositorySummaryQueryGateway::filterOptions() const {
    return m_repository->fetchFilterOptions();
}

QString RepositorySummaryQueryGateway::lastError() const {
    return m_repository == nullptr ? QString() : m_repository->lastError();
}

RepositoryStatQueryGateway::RepositoryStatQueryGateway(
    std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> repository)
    : m_repository(std::move(repository)) {}

BadStatPageData RepositoryStatQueryGateway::queryBadStatistics(const LaserSpc::Domain::BadStatQuery& query) const {
    BadStatPageData data;
    data.totalBadPoints = m_repository->fetchBadPointTotal(query);
    data.totalGradePoints = m_repository->fetchGradeTotal(query);
    data.badPoints = m_repository->fetchBadPointStats(query);
    data.grades = m_repository->fetchGradeStats(query);
    return data;
}

QString RepositoryStatQueryGateway::lastError() const {
    return m_repository == nullptr ? QString() : m_repository->lastError();
}

RepositoryRecordQueryGateway::RepositoryRecordQueryGateway(
    std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> repository)
    : m_repository(std::move(repository)) {}

LaserSpc::Domain::PageResult<LaserSpc::Domain::BoardRecordRow> RepositoryRecordQueryGateway::queryBoardRecords(
    const LaserSpc::Domain::BoardRecordQuery& query) const {
    return m_repository->fetchBoardRecords(query);
}

LaserSpc::Domain::PageResult<LaserSpc::Domain::PointRecordRow> RepositoryRecordQueryGateway::queryPointRecords(
    const LaserSpc::Domain::PointRecordQuery& query) const {
    return m_repository->fetchPointRecords(query);
}

QString RepositoryRecordQueryGateway::lastError() const {
    return m_repository == nullptr ? QString() : m_repository->lastError();
}

}  // namespace LaserSpc::App
