#pragma once

#include <QVariant>

#include "domain/CommandRepository.h"
#include "infrastructure/DatabaseConnection.h"

class QSqlQuery;

namespace LaserSpc::Infrastructure {

class MySqlSpcWriteRepository : public LaserSpc::Domain::ISpcWriteRepository {
public:
    explicit MySqlSpcWriteRepository(DatabaseSettings settings);

    QString lastError() const override;
    bool upsertBoardRecord(const LaserSpc::Domain::BoardRecordRow& row) override;
    bool insertPointRecord(const LaserSpc::Domain::PointRecordRow& row) override;
    bool replaceInspectionBatch(const LaserSpc::Domain::BoardRecordRow& board,
                                const QList<LaserSpc::Domain::PointRecordRow>& points) override;

private:
    DatabaseConnection::ConnectionLease database() const;
    bool exec(QSqlQuery& query, const QString& sql, const QList<QVariant>& values, const QString& label) const;

    DatabaseSettings m_settings;
    mutable QString m_lastError;
};

}  // namespace LaserSpc::Infrastructure
