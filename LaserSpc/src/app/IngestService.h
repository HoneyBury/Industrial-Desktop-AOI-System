#pragma once

#include <functional>
#include <memory>

#include "domain/CommandRepository.h"

namespace LaserSpc::App {

class IngestService {
public:
    explicit IngestService(std::shared_ptr<LaserSpc::Domain::ISpcWriteRepository> repository);

    LaserSpc::Domain::IngestResult ingestBoardRecord(LaserSpc::Domain::BoardRecordRow row) const;
    LaserSpc::Domain::IngestResult ingestPointRecord(LaserSpc::Domain::PointRecordRow row) const;
    LaserSpc::Domain::IngestResult ingestInspectionBatch(LaserSpc::Domain::InspectionBatch batch) const;
    void setMesForwarder(std::function<bool(const LaserSpc::Domain::InspectionBatch&, QString*)> forwarder);

private:
    bool normalizeBoard(LaserSpc::Domain::BoardRecordRow& row, QString* errorMessage) const;
    bool normalizePoint(LaserSpc::Domain::PointRecordRow& row, QString* errorMessage) const;

    std::shared_ptr<LaserSpc::Domain::ISpcWriteRepository> m_repository;
    std::function<bool(const LaserSpc::Domain::InspectionBatch&, QString*)> m_mesForwarder;
};

}  // namespace LaserSpc::App
