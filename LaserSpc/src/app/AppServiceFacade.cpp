#include "app/AppServiceFacade.h"

#include <QMutexLocker>

#include "app/QueryGateways.h"
#include "infrastructure/Logger.h"

namespace LaserSpc::App {

AppServiceFacade::AppServiceFacade(std::unique_ptr<LaserSpc::Domain::ISpcQueryRepository> repository,
                                   QString dataSourceMode)
    : m_repository(std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository>(std::move(repository))) {
    LaserSpc::Infrastructure::AppConfigService configService;
    m_settings = configService.settings();
    m_dataSourceMode = dataSourceMode.isEmpty() ? configService.dataSourceMode() : std::move(dataSourceMode);
}

SummaryQueryService AppServiceFacade::summaryQueryService() const {
    return SummaryQueryService(std::make_shared<RepositorySummaryQueryGateway>(currentRepository()));
}

StatQueryService AppServiceFacade::statQueryService() const {
    return StatQueryService(std::make_shared<RepositoryStatQueryGateway>(currentRepository()));
}

RecordQueryService AppServiceFacade::recordQueryService() const {
    return RecordQueryService(std::make_shared<RepositoryRecordQueryGateway>(currentRepository()));
}

LaserSpc::Domain::FilterCriteria AppServiceFacade::defaultFilter() const {
    QMutexLocker locker(&m_mutex);
    return LaserSpc::Infrastructure::AppConfigService::buildDefaultFilter(m_settings);
}

QString AppServiceFacade::applicationName() const {
    return QObject::tr("LaserSpc");
}

QString AppServiceFacade::dataSourceMode() const {
    QMutexLocker locker(&m_mutex);
    return m_dataSourceMode;
}

LaserSpc::Infrastructure::AppSettings AppServiceFacade::settings() const {
    QMutexLocker locker(&m_mutex);
    return m_settings;
}

LaserSpc::Domain::LaserContentDuplicateCheckResult AppServiceFacade::checkLaserContentDuplicate(
    const QString& laserContent) const {
    return currentRepository()->checkLaserContentDuplicate(laserContent);
}

void AppServiceFacade::replaceRepository(std::unique_ptr<LaserSpc::Domain::ISpcQueryRepository> repository,
                                         const QString& dataSourceMode,
                                         const LaserSpc::Infrastructure::AppSettings& settings) {
    QMutexLocker locker(&m_mutex);
    m_repository = std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository>(std::move(repository));
    m_settings = settings;
    m_dataSourceMode = dataSourceMode;
    Infrastructure::Logger::info("AppServiceFacade replaced repository with " + dataSourceMode);
}

std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> AppServiceFacade::currentRepository() const {
    QMutexLocker locker(&m_mutex);
    return m_repository;
}

}  // namespace LaserSpc::App
