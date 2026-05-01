#pragma once

#include <memory>

#include "app/QueryData.h"
#include "domain/Repository.h"

namespace LaserSpc::App {

class ISummaryQueryGateway {
public:
    virtual ~ISummaryQueryGateway() = default;
    virtual SummaryPageData querySummary(const LaserSpc::Domain::SummaryQuery& query) const = 0;
    virtual LaserSpc::Domain::FilterOptions filterOptions() const = 0;
    virtual QString lastError() const = 0;
};

class IStatQueryGateway {
public:
    virtual ~IStatQueryGateway() = default;
    virtual BadStatPageData queryBadStatistics(const LaserSpc::Domain::BadStatQuery& query) const = 0;
    virtual QString lastError() const = 0;
};

class IRecordQueryGateway {
public:
    virtual ~IRecordQueryGateway() = default;
    virtual LaserSpc::Domain::PageResult<LaserSpc::Domain::BoardRecordRow> queryBoardRecords(
        const LaserSpc::Domain::BoardRecordQuery& query) const = 0;
    virtual LaserSpc::Domain::PageResult<LaserSpc::Domain::PointRecordRow> queryPointRecords(
        const LaserSpc::Domain::PointRecordQuery& query) const = 0;
    virtual QString lastError() const = 0;
};

class RepositorySummaryQueryGateway final : public ISummaryQueryGateway {
public:
    explicit RepositorySummaryQueryGateway(std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> repository);

    SummaryPageData querySummary(const LaserSpc::Domain::SummaryQuery& query) const override;
    LaserSpc::Domain::FilterOptions filterOptions() const override;
    QString lastError() const override;

private:
    std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> m_repository;
};

class RepositoryStatQueryGateway final : public IStatQueryGateway {
public:
    explicit RepositoryStatQueryGateway(std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> repository);

    BadStatPageData queryBadStatistics(const LaserSpc::Domain::BadStatQuery& query) const override;
    QString lastError() const override;

private:
    std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> m_repository;
};

class RepositoryRecordQueryGateway final : public IRecordQueryGateway {
public:
    explicit RepositoryRecordQueryGateway(std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> repository);

    LaserSpc::Domain::PageResult<LaserSpc::Domain::BoardRecordRow> queryBoardRecords(
        const LaserSpc::Domain::BoardRecordQuery& query) const override;
    LaserSpc::Domain::PageResult<LaserSpc::Domain::PointRecordRow> queryPointRecords(
        const LaserSpc::Domain::PointRecordQuery& query) const override;
    QString lastError() const override;

private:
    std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> m_repository;
};

}  // namespace LaserSpc::App
