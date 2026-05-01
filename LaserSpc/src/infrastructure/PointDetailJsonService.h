#pragma once

#include <QJsonObject>
#include <QString>

#include "domain/Models.h"

namespace LaserSpc::Infrastructure {

class PointDetailJsonService {
public:
    static QString algorithmPlanKey();
    static QString algorithmPlanAlias();
    static QString fieldDisplayNamesKey();
    static QString defaultDetailDirectory();
    static QString buildDefaultFilePath(const LaserSpc::Domain::PointRecordRow& row,
                                        const QString& baseDirectory = QString());
    static QString resolveDetailFilePath(const QString& filePath);
    static bool saveDetail(const LaserSpc::Domain::PointDetailInfo& detail,
                           const QString& filePath,
                           QString* errorMessage);
    static bool loadDetail(const QString& filePath,
                           LaserSpc::Domain::PointDetailInfo* detail,
                           QString* errorMessage,
                           QJsonObject* rawObject = nullptr);

    static QJsonObject toJsonObject(const LaserSpc::Domain::PointDetailInfo& detail);
    static bool fromJsonObject(const QJsonObject& object,
                               LaserSpc::Domain::PointDetailInfo* detail,
                               QString* errorMessage = nullptr);
};

}  // namespace LaserSpc::Infrastructure
