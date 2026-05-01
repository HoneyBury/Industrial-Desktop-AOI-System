#pragma once

#include <QFileInfo>
#include <QStringList>
#include <QString>

#include "infrastructure/AppConfigService.h"
#include "domain/Models.h"

class QWidget;
class QPixmap;

namespace LaserSpc::Infrastructure {

struct ExportReportBundleOptions {
    QString reportName;
    QString reportType;
    QString currentTaskState;
    QStringList selectedFiles;
    QStringList notes;
    QStringList criteriaSummary;
    QStringList metricSummary;
    ReportTemplateSettings templateSettings;
};

class ExportService {
public:
    static QString exportRootDirectory();
    static QString defaultExportDirectory();
    static bool exportSummaryRowsToCsv(const QList<LaserSpc::Domain::SummaryRow>& rows,
                                       QString* outputPath,
                                       QString* errorMessage);
    static bool exportBadStatsToCsv(const QList<LaserSpc::Domain::BadPointStatRow>& badPoints,
                                    const QList<LaserSpc::Domain::GradeStatRow>& grades,
                                    QString* outputPath,
                                    QString* errorMessage);
    static bool exportBoardRecordsToCsv(const QList<LaserSpc::Domain::BoardRecordRow>& rows,
                                        QString* outputPath,
                                        QString* errorMessage);
    static bool exportPointRecordsToCsv(const QList<LaserSpc::Domain::PointRecordRow>& rows,
                                        QString* outputPath,
                                        QString* errorMessage);
    static bool exportWidgetScreenshot(QWidget* widget,
                                       const QString& prefix,
                                       QString* outputPath,
                                       QString* errorMessage);
    static bool exportPixmapScreenshot(const QPixmap& pixmap,
                                       const QString& prefix,
                                       QString* outputPath,
                                       QString* errorMessage);
    static QList<QFileInfo> recentExportFiles(int maxCount = 10);
    static bool removeExportFiles(const QStringList& absolutePaths, QString* errorMessage);
    static bool createReportBundle(const ExportReportBundleOptions& options,
                                   QString* outputPath,
                                   QString* errorMessage);
};

}  // namespace LaserSpc::Infrastructure
