#pragma once

#include "domain/Models.h"

namespace LaserSpc::Domain {

class ISpcWriteRepository {
public:
    virtual ~ISpcWriteRepository() = default;

    virtual QString lastError() const = 0;
    virtual bool upsertBoardRecord(const BoardRecordRow& row) = 0;
    virtual bool insertPointRecord(const PointRecordRow& row) = 0;
    virtual bool replaceInspectionBatch(const BoardRecordRow& board, const QList<PointRecordRow>& points) = 0;
};

}  // namespace LaserSpc::Domain
