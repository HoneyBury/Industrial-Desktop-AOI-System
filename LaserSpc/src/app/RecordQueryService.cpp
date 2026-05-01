#include "app/RecordQueryService.h"

#include "app/QueryExecutionSupport.h"

namespace LaserSpc::App {

RecordQueryService::RecordQueryService(std::shared_ptr<IRecordQueryGateway> gateway)
    : m_gateway(std::move(gateway)) {}

LaserSpc::Domain::PageResult<LaserSpc::Domain::BoardRecordRow> RecordQueryService::queryBoardRecords(
    const LaserSpc::Domain::BoardRecordQuery& query) const {
    return executeQueryWithPerf("RecordQueryService.queryBoardRecords", [&]() {
        return m_gateway->queryBoardRecords(query);
    }, [](const LaserSpc::Domain::PageResult<LaserSpc::Domain::BoardRecordRow>& result) {
        return QString("rows=%1 total=%2 page=%3 size=%4")
                                     .arg(result.rows.size())
                                     .arg(result.total)
                                     .arg(result.page)
                                     .arg(result.pageSize);
    });
}

LaserSpc::Domain::PageResult<LaserSpc::Domain::PointRecordRow> RecordQueryService::queryPointRecords(
    const LaserSpc::Domain::PointRecordQuery& query) const {
    return executeQueryWithPerf("RecordQueryService.queryPointRecords", [&]() {
        return m_gateway->queryPointRecords(query);
    }, [](const LaserSpc::Domain::PageResult<LaserSpc::Domain::PointRecordRow>& result) {
        return QString("rows=%1 total=%2 page=%3 size=%4")
                                     .arg(result.rows.size())
                                     .arg(result.total)
                                     .arg(result.page)
                                     .arg(result.pageSize);
    });
}

QString RecordQueryService::lastRepositoryError() const {
    return m_gateway == nullptr ? QString() : m_gateway->lastError();
}

}  // namespace LaserSpc::App
