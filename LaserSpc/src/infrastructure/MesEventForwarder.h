#pragma once

#include "domain/Models.h"
#include "infrastructure/AppConfigService.h"

namespace LaserSpc::Infrastructure {

class MesEventForwarder {
public:
    explicit MesEventForwarder(MesSettings settings);

    bool forwardInspectionBatch(const LaserSpc::Domain::InspectionBatch& batch, QString* errorMessage) const;

private:
    MesSettings m_settings;
};

}  // namespace LaserSpc::Infrastructure
