#include "infrastructure/RepositoryFactory.h"

#include "infrastructure/Logger.h"
#include "infrastructure/MockSpcRepository.h"
#include "infrastructure/MySqlSpcRepository.h"

namespace LaserSpc::Infrastructure {

RepositoryBuildResult RepositoryFactory::build(const AppSettings& settings) {
    RepositoryBuildResult result;

    if (settings.useMySql) {
        Logger::info(QString("RepositoryFactory trying MySQL repository at %1:%2/%3")
                         .arg(settings.database.host)
                         .arg(settings.database.port)
                         .arg(settings.database.databaseName));
        auto mysqlRepository = std::make_unique<MySqlSpcRepository>(settings.database);
        if (mysqlRepository->isAvailable()) {
            result.dataSourceMode = "MySQL Repository";
            result.repository = std::move(mysqlRepository);
            Logger::info("RepositoryFactory selected MySQL repository.");
            return result;
        }

        const QString unavailableMessage = "MySQL repository unavailable: " + mysqlRepository->lastError();
        if (settings.allowMockFallback) {
            result.warningMessage = unavailableMessage + " | fallback to mock repository enabled.";
            Logger::warn(result.warningMessage);
        } else {
            result.warningMessage = unavailableMessage + " | keeping MySQL repository in strict mode.";
            result.dataSourceMode = "MySQL Repository (Unavailable)";
            result.repository = std::move(mysqlRepository);
            Logger::warn(result.warningMessage);
            return result;
        }
    }

    result.dataSourceMode = settings.useMySql ? "Mock Repository (MySQL fallback failed)" : "Mock Repository";
    result.repository = std::make_unique<MockSpcRepository>();
    Logger::info("RepositoryFactory selected mock repository.");
    return result;
}

}  // namespace LaserSpc::Infrastructure
