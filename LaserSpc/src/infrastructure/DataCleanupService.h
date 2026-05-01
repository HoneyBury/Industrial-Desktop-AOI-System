#pragma once

#include <QDateTime>
#include <QString>

#include "infrastructure/DatabaseConnection.h"

namespace LaserSpc::Infrastructure {

struct DataCleanupResult {
    bool success = false;
    int deletedPointRecords = 0;
    int deletedBoardRecords = 0;
    int processedBatches = 0;
    int retryCount = 0;
    QDateTime cutoffTime;
    QString errorMessage;
};

class DataCleanupService {
public:
    static DataCleanupResult cleanupSeedData(const DatabaseSettings& settings);
    static DataCleanupResult cleanupProductionDataOlderThan(const DatabaseSettings& settings, int retentionDays);
};

}  // namespace LaserSpc::Infrastructure
