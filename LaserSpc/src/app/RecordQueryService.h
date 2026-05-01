#pragma once

#include <memory>

#include "app/QueryGateways.h"

namespace LaserSpc::App {

class RecordQueryService {
public:
    explicit RecordQueryService(std::shared_ptr<IRecordQueryGateway> gateway);

    LaserSpc::Domain::PageResult<LaserSpc::Domain::BoardRecordRow> queryBoardRecords(
        const LaserSpc::Domain::BoardRecordQuery& query) const;
    LaserSpc::Domain::PageResult<LaserSpc::Domain::PointRecordRow> queryPointRecords(
        const LaserSpc::Domain::PointRecordQuery& query) const;
    QString lastRepositoryError() const;

private:
    std::shared_ptr<IRecordQueryGateway> m_gateway;
};

}  // namespace LaserSpc::App
