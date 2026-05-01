#include "app/SummaryQueryService.h"

#include "app/QueryExecutionSupport.h"

namespace LaserSpc::App {

SummaryQueryService::SummaryQueryService(std::shared_ptr<ISummaryQueryGateway> gateway)
    : m_gateway(std::move(gateway)) {}

SummaryPageData SummaryQueryService::querySummary(const LaserSpc::Domain::SummaryQuery& query) const {
    return executeQueryWithPerf("SummaryQueryService.querySummary", [&]() {
        return m_gateway->querySummary(query);
    }, [](const SummaryPageData& data) {
        return QString("metrics=%1 rows=%2 total=%3")
                                     .arg(data.metrics.size())
                                     .arg(data.table.rows.size())
                                     .arg(data.table.total);
    });
}

LaserSpc::Domain::FilterOptions SummaryQueryService::filterOptions() const {
    return executeQueryWithPerf("SummaryQueryService.filterOptions", [&]() {
        return m_gateway->filterOptions();
    }, [](const LaserSpc::Domain::FilterOptions& options) {
        return QString("lines=%1 programs=%2 devices=%3")
                                     .arg(options.lineNames.size())
                                     .arg(options.programNames.size())
                                     .arg(options.deviceNames.size());
    });
}

QString SummaryQueryService::lastRepositoryError() const {
    return m_gateway == nullptr ? QString() : m_gateway->lastError();
}

}  // namespace LaserSpc::App
