#include "app/StatQueryService.h"

#include "app/QueryExecutionSupport.h"

namespace LaserSpc::App {

StatQueryService::StatQueryService(std::shared_ptr<IStatQueryGateway> gateway)
    : m_gateway(std::move(gateway)) {}

BadStatPageData StatQueryService::queryBadStatistics(const LaserSpc::Domain::BadStatQuery& query) const {
    return executeQueryWithPerf("StatQueryService.queryBadStatistics", [&]() {
        return m_gateway->queryBadStatistics(query);
    }, [](const BadStatPageData& data) {
        return QString("badPoints=%1 grades=%2")
                                     .arg(data.badPoints.size())
                                     .arg(data.grades.size());
    });
}

QString StatQueryService::lastRepositoryError() const {
    return m_gateway == nullptr ? QString() : m_gateway->lastError();
}

}  // namespace LaserSpc::App
