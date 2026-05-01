#pragma once

#include <QMutex>
#include <memory>

#include "app/QueryData.h"
#include "app/RecordQueryService.h"
#include "app/StatQueryService.h"
#include "app/SummaryQueryService.h"
#include "domain/Repository.h"
#include "infrastructure/AppConfigService.h"

namespace LaserSpc::App {

class AppServiceFacade {
public:
    explicit AppServiceFacade(std::unique_ptr<LaserSpc::Domain::ISpcQueryRepository> repository,
                              QString dataSourceMode = QString());

    SummaryQueryService summaryQueryService() const;
    StatQueryService statQueryService() const;
    RecordQueryService recordQueryService() const;

    LaserSpc::Domain::FilterCriteria defaultFilter() const;
    QString applicationName() const;
    QString dataSourceMode() const;
    LaserSpc::Infrastructure::AppSettings settings() const;
    LaserSpc::Domain::LaserContentDuplicateCheckResult checkLaserContentDuplicate(const QString& laserContent) const;
    void replaceRepository(std::unique_ptr<LaserSpc::Domain::ISpcQueryRepository> repository,
                           const QString& dataSourceMode,
                           const LaserSpc::Infrastructure::AppSettings& settings);

private:
    std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> currentRepository() const;

    std::shared_ptr<LaserSpc::Domain::ISpcQueryRepository> m_repository;
    LaserSpc::Infrastructure::AppSettings m_settings;
    QString m_dataSourceMode;
    mutable QMutex m_mutex;
};

}  // namespace LaserSpc::App
