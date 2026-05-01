#pragma once

#include <memory>

#include "domain/Repository.h"
#include "infrastructure/AppConfigService.h"

namespace LaserSpc::Infrastructure {

struct RepositoryBuildResult {
    std::unique_ptr<LaserSpc::Domain::ISpcQueryRepository> repository;
    QString dataSourceMode;
    QString warningMessage;
};

class RepositoryFactory {
public:
    static RepositoryBuildResult build(const AppSettings& settings);
};

}  // namespace LaserSpc::Infrastructure
