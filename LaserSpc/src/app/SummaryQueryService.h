#pragma once

#include <memory>

#include "app/QueryData.h"
#include "app/QueryGateways.h"

namespace LaserSpc::App {

class SummaryQueryService {
public:
    explicit SummaryQueryService(std::shared_ptr<ISummaryQueryGateway> gateway);

    SummaryPageData querySummary(const LaserSpc::Domain::SummaryQuery& query) const;
    LaserSpc::Domain::FilterOptions filterOptions() const;
    QString lastRepositoryError() const;

private:
    std::shared_ptr<ISummaryQueryGateway> m_gateway;
};

}  // namespace LaserSpc::App
