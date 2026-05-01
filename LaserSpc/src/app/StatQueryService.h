#pragma once

#include <memory>

#include "app/QueryData.h"
#include "app/QueryGateways.h"

namespace LaserSpc::App {

class StatQueryService {
public:
    explicit StatQueryService(std::shared_ptr<IStatQueryGateway> gateway);

    BadStatPageData queryBadStatistics(const LaserSpc::Domain::BadStatQuery& query) const;
    QString lastRepositoryError() const;

private:
    std::shared_ptr<IStatQueryGateway> m_gateway;
};

}  // namespace LaserSpc::App
