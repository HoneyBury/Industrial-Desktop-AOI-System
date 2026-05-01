#include "infrastructure/ExportService.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QFont>
#include <QObject>
#include <QPdfWriter>
#include <QPixmap>
#include <QTextDocument>
#include <QSaveFile>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif
#include <QTextStream>
#include <QWidget>

#include "infrastructure/AppConfigService.h"
#include "infrastructure/Logger.h"

namespace {

QString timestampSuffix() {
    return QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
}

QString exportDateFolder() {
    return QDateTime::currentDateTime().toString("yyyyMMdd");
}

QString buildExportPath(const QString& prefix, const QString& extension) {
    const QString directory = LaserSpc::Infrastructure::ExportService::defaultExportDirectory();
    return QDir(directory).filePath(QString("laser_spc_%1_%2.%3").arg(prefix, timestampSuffix(), extension));
}

QString csvEscape(const QString& value) {
    QString escaped = value;
    escaped.replace('"', "\"\"");
    return "\"" + escaped + "\"";
}

QString htmlEscape(const QString& value) {
    QString escaped = value;
    escaped.replace('&', "&amp;");
    escaped.replace('<', "&lt;");
    escaped.replace('>', "&gt;");
    escaped.replace('"', "&quot;");
    return escaped;
}

void configureUtf8(QTextStream& stream) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#else
    stream.setEncoding(QStringConverter::Utf8);
#endif
}

void writeUtf8Bom(QTextStream& stream) {
    stream << QChar(0xFEFF);
}

bool prepareSaveFile(QSaveFile& file, QString* errorMessage) {
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = file.errorString();
    }
    return false;
}

QString detectReportType(const LaserSpc::Infrastructure::ExportReportBundleOptions& options) {
    if (!options.reportType.trimmed().isEmpty()) {
        return options.reportType.trimmed().toLower();
    }

    for (const QString& path : options.selectedFiles) {
        const QString fileName = QFileInfo(path).fileName().toLower();
        if (fileName.contains("summary")) {
            return QStringLiteral("summary");
        }
        if (fileName.contains("bad") || fileName.contains(QStringLiteral("stat"))) {
            return QStringLiteral("bad_stats");
        }
        if (fileName.contains("board")) {
            return QStringLiteral("board_records");
        }
        if (fileName.contains("point")) {
            return QStringLiteral("point_records");
        }
    }

    return QStringLiteral("general");
}

QString reportTitleForType(const QString& reportType) {
    if (reportType.contains(QStringLiteral("summary")) || reportType.contains(QStringLiteral("csv"))) {
        return QObject::tr("数据总览报告");
    }
    if (reportType.contains(QStringLiteral("bad")) || reportType.contains(QStringLiteral("stat"))) {
        return QObject::tr("不良统计报告");
    }
    if (reportType.contains(QStringLiteral("board"))) {
        return QObject::tr("单板记录报告");
    }
    if (reportType.contains(QStringLiteral("point")) || reportType.contains(QStringLiteral("png"))) {
        return QObject::tr("点位记录报告");
    }
    return QObject::tr("SPC 交付报告");
}

QString reportDescriptionForType(const QString& reportType) {
    if (reportType.contains(QObject::tr("summary")) || reportType.contains(QObject::tr("csv"))) {
        return QObject::tr("聚焦当前筛选范围内的总板数、良率与线体/程序/设备汇总结果。");
    }
    if (reportType.contains(QObject::tr("bad")) || reportType.contains(QObject::tr("stat"))) {
        return QObject::tr("聚焦 Top 不良点、读码等级分布与统计图表输出。");
    }
    if (reportType.contains(QObject::tr("board"))) {
        return QObject::tr("聚焦板级记录、结果分布和现场追溯信息。");
    }
    if (reportType.contains(QObject::tr("point")) || reportType.contains(QObject::tr("png"))) {
        return QObject::tr("聚焦点位级记录、时间顺序和异常点位追踪。");
    }
    return QObject::tr("包含当前导出任务对应的交付文件、说明和可打印总览。");
}

QString buildOverviewHtml(const QString& title,
                          const QString& description,
                          const QString& generatedAt,
                          const QString& currentTaskState,
                          const QStringList& copiedFiles,
                          const QStringList& notes,
                          const QStringList& criteriaSummary,
                          const QStringList& metricSummary,
                          const LaserSpc::Infrastructure::ReportTemplateSettings& templateSettings,
                          const QString& bundleDirectory) {
    QString html;
    QTextStream stream(&html);
    stream << "<!doctype html><html><head><meta charset=\"utf-8\">";
    stream << "<title>" << htmlEscape(title) << "</title>";
    stream << "<style>"
              "body{font-family:'Microsoft YaHei',sans-serif;background:#f5f7fb;color:#1f2937;margin:0;padding:32px;}"
              ".wrap{max-width:960px;margin:0 auto;background:#fff;border-radius:16px;padding:28px;box-shadow:0 12px 40px rgba(15,23,42,.08);}"
              "h1{margin:0 0 8px;font-size:28px;}h2{margin:28px 0 12px;font-size:18px;}"
              "p,li{line-height:1.7;}.meta{color:#6b7280;margin-bottom:20px;}"
              ".tag{display:inline-block;background:#e0f2fe;color:#075985;border-radius:999px;padding:4px 10px;margin-right:8px;font-size:12px;}"
              "table{width:100%;border-collapse:collapse;margin-top:12px;}th,td{border-bottom:1px solid #e5e7eb;padding:10px;text-align:left;}"
              "code{background:#eef2ff;padding:2px 6px;border-radius:6px;}"
              "</style></head><body><div class=\"wrap\">";
    if (!templateSettings.logoPath.isEmpty() && QFileInfo::exists(templateSettings.logoPath)) {
        const QString logoName = QFileInfo(templateSettings.logoPath).fileName();
        const QString targetLogoPath = QDir(bundleDirectory).filePath(logoName);
        if (!QFileInfo::exists(targetLogoPath)) {
            QFile::remove(targetLogoPath);
            QFile::copy(templateSettings.logoPath, targetLogoPath);
        }
        stream << "<div><img src=\"" << htmlEscape(logoName)
               << "\" alt=\"logo\" style=\"max-height:56px;margin-bottom:16px;\"></div>";
    }
    stream << "<h1>" << htmlEscape(templateSettings.reportTitle.isEmpty() ? title : templateSettings.reportTitle) << "</h1>";
    if (!templateSettings.customerName.isEmpty()) {
        stream << "<div class=\"meta\">" << htmlEscape(QObject::tr("客户：")) << htmlEscape(templateSettings.customerName)
               << "</div>";
    }
    stream << "<div class=\"meta\">" << htmlEscape(QObject::tr("生成时间：")) << htmlEscape(generatedAt) << "</div>";
    stream << "<p>" << htmlEscape(description) << "</p>";
    stream << "<div><span class=\"tag\">" << htmlEscape(QObject::tr("文件数：")) << copiedFiles.size() << "</span>";
    stream << "<span class=\"tag\">"
           << htmlEscape(QObject::tr("任务状态："))
           << htmlEscape(currentTaskState.isEmpty() ? QObject::tr("空闲") : currentTaskState)
           << "</span></div>";
    if (!criteriaSummary.isEmpty()) {
        stream << "<h2>" << htmlEscape(QObject::tr("筛选条件")) << "</h2><ul>";
        for (const QString& item : criteriaSummary) {
            stream << "<li>" << htmlEscape(item) << "</li>";
        }
        stream << "</ul>";
    }
    if (!metricSummary.isEmpty()) {
        stream << "<h2>" << htmlEscape(QObject::tr("统计摘要")) << "</h2><ul>";
        for (const QString& item : metricSummary) {
            stream << "<li>" << htmlEscape(item) << "</li>";
        }
        stream << "</ul>";
    }
    stream << "<h2>" << htmlEscape(QObject::tr("导出文件")) << "</h2>"
           << "<table><thead><tr><th>" << htmlEscape(QObject::tr("文件名")) << "</th><th>"
           << htmlEscape(QObject::tr("类型")) << "</th></tr></thead><tbody>";
    for (const QString& filePath : copiedFiles) {
        const QFileInfo fileInfo(filePath);
        stream << "<tr><td><a href=\"" << htmlEscape(fileInfo.fileName()) << "\">" << htmlEscape(fileInfo.fileName())
               << "</a></td><td><code>" << htmlEscape(fileInfo.suffix().toUpper()) << "</code></td></tr>";
    }
    stream << "</tbody></table>";
    if (!notes.isEmpty()) {
        stream << "<h2>" << htmlEscape(QObject::tr("备注")) << "</h2><ul>";
        for (const QString& note : notes) {
            stream << "<li>" << htmlEscape(note) << "</li>";
        }
        stream << "</ul>";
    }
    if (!templateSettings.footerText.isEmpty()) {
        stream << "<p style=\"margin-top:24px;color:#6b7280;border-top:1px solid #e5e7eb;padding-top:16px;\">"
               << htmlEscape(templateSettings.footerText) << "</p>";
    }
    stream << "</div></body></html>";
    return html;
}

bool writeTextFile(const QString& filePath, const QString& content, QString* errorMessage) {
    QSaveFile file(filePath);
    if (!prepareSaveFile(file, errorMessage)) {
        return false;
    }

    QTextStream stream(&file);
    configureUtf8(stream);
    writeUtf8Bom(stream);
    stream << content;
    if (!file.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    return true;
}

}  // namespace

namespace LaserSpc::Infrastructure {

QString ExportService::exportRootDirectory() {
    const QString overrideDirectory = qEnvironmentVariable("LASERSPC_EXPORT_DIR");
    if (!overrideDirectory.isEmpty()) {
        QDir dir(overrideDirectory);
        dir.mkpath(".");
        return dir.absolutePath();
    }

    const QString configuredDirectory = AppConfigService().settings().exportDirectory.trimmed();
    if (!configuredDirectory.isEmpty()) {
        QDir dir(configuredDirectory);
        dir.mkpath(".");
        return dir.absolutePath();
    }

    QDir dir(QCoreApplication::applicationDirPath());
    if (!dir.exists("exports")) {
        dir.mkpath("exports");
    }
    return dir.filePath("exports");
}

QString ExportService::defaultExportDirectory() {
    QDir dir(exportRootDirectory());
    const QString datedFolder = dir.filePath(exportDateFolder());
    dir.mkpath(exportDateFolder());
    return datedFolder;
}

bool ExportService::exportSummaryRowsToCsv(const QList<LaserSpc::Domain::SummaryRow>& rows,
                                           QString* outputPath,
                                           QString* errorMessage) {
    const QString filePath = buildExportPath("summary", "csv");
    QSaveFile file(filePath);
    if (!prepareSaveFile(file, errorMessage)) {
        Logger::warn("Export summary CSV failed: " + file.errorString());
        return false;
    }

    QTextStream stream(&file);
    configureUtf8(stream);
    writeUtf8Bom(stream);
    stream << QObject::tr("程序名,线体,设备,总板数,良板数,不良板数,良率,最近更新时间") << "\n";

    for (const auto& row : rows) {
        stream << csvEscape(row.programName) << ","
               << csvEscape(row.lineName) << ","
               << csvEscape(row.deviceName) << ","
               << row.totalBoards << ","
               << row.goodBoards << ","
               << row.badBoards << ","
               << QString::number(row.yieldRate, 'f', 2) << ","
               << csvEscape(row.lastUpdated.toString("yyyy-MM-dd HH:mm:ss")) << "\n";
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        Logger::warn("Commit summary CSV failed: " + file.errorString());
        return false;
    }

    if (outputPath) {
        *outputPath = filePath;
    }
    Logger::info("Exported summary CSV: " + filePath);
    return true;
}

bool ExportService::exportBadStatsToCsv(const QList<LaserSpc::Domain::BadPointStatRow>& badPoints,
                                        const QList<LaserSpc::Domain::GradeStatRow>& grades,
                                        QString* outputPath,
                                        QString* errorMessage) {
    const QString filePath = buildExportPath("bad_stats", "csv");
    QSaveFile file(filePath);
    if (!prepareSaveFile(file, errorMessage)) {
        Logger::warn("Export bad stats CSV failed: " + file.errorString());
        return false;
    }

    QTextStream stream(&file);
    configureUtf8(stream);
    writeUtf8Bom(stream);
    stream << QObject::tr("Top不良点") << "\n";
    stream << QObject::tr("不良点,次数,占比") << "\n";
    for (const auto& row : badPoints) {
        stream << csvEscape(row.badPointName) << ","
               << row.count << ","
               << QString::number(row.ratio, 'f', 2) << "\n";
    }

    stream << "\n" << QObject::tr("读码等级分布") << "\n";
    stream << QObject::tr("等级,数量,占比") << "\n";
    for (const auto& row : grades) {
        stream << csvEscape(row.grade) << ","
               << row.count << ","
               << QString::number(row.ratio, 'f', 2) << "\n";
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        Logger::warn("Commit bad stats CSV failed: " + file.errorString());
        return false;
    }

    if (outputPath) {
        *outputPath = filePath;
    }
    Logger::info("Exported bad stats CSV: " + filePath);
    return true;
}

bool ExportService::exportBoardRecordsToCsv(const QList<LaserSpc::Domain::BoardRecordRow>& rows,
                                            QString* outputPath,
                                            QString* errorMessage) {
    const QString filePath = buildExportPath("board_records", "csv");
    QSaveFile file(filePath);
    if (!prepareSaveFile(file, errorMessage)) {
        Logger::warn("Export board records CSV failed: " + file.errorString());
        return false;
    }

    QTextStream stream(&file);
    configureUtf8(stream);
    writeUtf8Bom(stream);
    stream << QObject::tr("程序名,板号,结果,线体,设备,操作员,时间") << "\n";
    for (const auto& row : rows) {
        stream << csvEscape(row.programName) << ","
               << csvEscape(row.boardCode) << ","
               << csvEscape(row.result) << ","
               << csvEscape(row.lineName) << ","
               << csvEscape(row.deviceName) << ","
               << csvEscape(row.operatorName) << ","
               << csvEscape(row.eventTime.toString("yyyy-MM-dd HH:mm:ss")) << "\n";
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        Logger::warn("Commit board records CSV failed: " + file.errorString());
        return false;
    }

    if (outputPath) {
        *outputPath = filePath;
    }
    Logger::info("Exported board records CSV: " + filePath);
    return true;
}

bool ExportService::exportPointRecordsToCsv(const QList<LaserSpc::Domain::PointRecordRow>& rows,
                                            QString* outputPath,
                                            QString* errorMessage) {
    const QString filePath = buildExportPath("point_records", "csv");
    QSaveFile file(filePath);
    if (!prepareSaveFile(file, errorMessage)) {
        Logger::warn("Export point records CSV failed: " + file.errorString());
        return false;
    }

    QTextStream stream(&file);
    configureUtf8(stream);
    writeUtf8Bom(stream);
    stream << QObject::tr("程序名,板号,点位,结果,读码等级,镭射内容,读码内容,是否镭射,是否读码,线体,开始时间,结束时间,设备,详情JSON路径") << "\n";
    for (const auto& row : rows) {
        stream << csvEscape(row.programName) << ","
               << csvEscape(row.boardCode) << ","
               << csvEscape(row.pointName) << ","
               << csvEscape(row.result) << ","
               << csvEscape(row.readGrade) << ","
               << csvEscape(row.laserContent) << ","
               << csvEscape(row.readCodeContent) << ","
               << csvEscape(row.isLaser ? QObject::tr("是") : QObject::tr("否")) << ","
               << csvEscape(row.isReadCode ? QObject::tr("是") : QObject::tr("否")) << ","
               << csvEscape(row.lineName) << ","
               << csvEscape(row.startTime.toString("yyyy-MM-dd HH:mm:ss")) << ","
               << csvEscape(row.endTime.toString("yyyy-MM-dd HH:mm:ss")) << ","
               << csvEscape(row.deviceName) << ","
               << csvEscape(row.detailJsonPath) << "\n";
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        Logger::warn("Commit point records CSV failed: " + file.errorString());
        return false;
    }

    if (outputPath) {
        *outputPath = filePath;
    }
    Logger::info("Exported point records CSV: " + filePath);
    return true;
}

bool ExportService::exportWidgetScreenshot(QWidget* widget,
                                           const QString& prefix,
                                           QString* outputPath,
                                           QString* errorMessage) {
    if (widget == nullptr) {
        if (errorMessage) {
            *errorMessage = "Widget is null.";
        }
        return false;
    }

    const QPixmap pixmap = widget->grab();
    return exportPixmapScreenshot(pixmap, prefix, outputPath, errorMessage);
}

bool ExportService::exportPixmapScreenshot(const QPixmap& pixmap,
                                           const QString& prefix,
                                           QString* outputPath,
                                           QString* errorMessage) {
    const QString filePath = buildExportPath(prefix, "png");
    if (pixmap.isNull()) {
        if (errorMessage) {
            *errorMessage = "Failed to capture widget screenshot.";
        }
        Logger::warn("Export screenshot failed: failed to capture widget.");
        return false;
    }

    if (!pixmap.save(filePath, "PNG")) {
        if (errorMessage) {
            *errorMessage = "Failed to save screenshot image.";
        }
        Logger::warn("Export screenshot failed: unable to save " + filePath);
        return false;
    }

    if (outputPath) {
        *outputPath = filePath;
    }
    Logger::info("Exported screenshot: " + filePath);
    return true;
}

QList<QFileInfo> ExportService::recentExportFiles(int maxCount) {
    QList<QFileInfo> files;
    if (maxCount <= 0) {
        return files;
    }

    QDir exportsDir(exportRootDirectory());
    if (!exportsDir.exists()) {
        return files;
    }

    const QFileInfoList datedFolders =
        exportsDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time | QDir::Reversed);

    for (auto it = datedFolders.crbegin(); it != datedFolders.crend() && files.size() < maxCount; ++it) {
        QDir dateDir(it->absoluteFilePath());
        const QFileInfoList dateFiles =
            dateDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
        for (const QFileInfo& file : dateFiles) {
            files.append(file);
            if (files.size() >= maxCount) {
                break;
            }
        }
    }

    std::sort(files.begin(), files.end(), [](const QFileInfo& left, const QFileInfo& right) {
        return left.lastModified() > right.lastModified();
    });
    return files;
}

bool ExportService::removeExportFiles(const QStringList& absolutePaths, QString* errorMessage) {
    QStringList failedPaths;
    for (const QString& path : absolutePaths) {
        if (path.isEmpty()) {
            continue;
        }

        QFile file(path);
        if (!file.exists()) {
            continue;
        }

        if (!file.remove()) {
            failedPaths.append(path);
        }
    }

    if (!failedPaths.isEmpty()) {
        const QString message = QString("Failed to remove export files: %1").arg(failedPaths.join(", "));
        if (errorMessage) {
            *errorMessage = message;
        }
        Logger::warn(message);
        return false;
    }

    Logger::info(QString("Removed %1 export file(s).").arg(absolutePaths.size()));
    return true;
}

bool ExportService::createReportBundle(const ExportReportBundleOptions& options,
                                       QString* outputPath,
                                       QString* errorMessage) {
    const QString sanitizedName = options.reportName.trimmed().isEmpty() ? QObject::tr("report_bundle")
                                                                         : options.reportName.trimmed();
    const QString timestamp = timestampSuffix();
    const QString bundleDirectory =
        QDir(defaultExportDirectory()).filePath(QObject::tr("%1_%2").arg(sanitizedName, timestamp));
    QDir().mkpath(bundleDirectory);

    QStringList copiedFiles;
    for (const QString& sourcePath : options.selectedFiles) {
        const QFileInfo sourceInfo(sourcePath);
        if (!sourceInfo.exists() || !sourceInfo.isFile()) {
            continue;
        }

        const QString targetPath = QDir(bundleDirectory).filePath(sourceInfo.fileName());
        QFile::remove(targetPath);
        if (!QFile::copy(sourcePath, targetPath)) {
            if (errorMessage != nullptr) {
                *errorMessage = QObject::tr("复制导出文件失败：%1").arg(sourceInfo.fileName());
            }
            return false;
        }
        copiedFiles.append(targetPath);
    }

    QSaveFile manifestFile(QDir(bundleDirectory).filePath(QObject::tr("report_manifest.txt")));
    if (!prepareSaveFile(manifestFile, errorMessage)) {
        return false;
    }

    QTextStream manifestStream(&manifestFile);
    configureUtf8(manifestStream);
    writeUtf8Bom(manifestStream);
    manifestStream << "LaserSpc Report Bundle\n";
    manifestStream << "Name=" << sanitizedName << "\n";
    manifestStream << "GeneratedAt=" << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    manifestStream << "CurrentTaskState=" << options.currentTaskState << "\n";
    manifestStream << "Files=" << copiedFiles.size() << "\n";
    for (const QString& filePath : copiedFiles) {
        manifestStream << "- " << QFileInfo(filePath).fileName() << "\n";
    }
    if (!options.notes.isEmpty()) {
        manifestStream << "Notes=\n";
        for (const QString& note : options.notes) {
            manifestStream << "* " << note << "\n";
        }
    }

    if (!manifestFile.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = manifestFile.errorString();
        }
        return false;
    }

    const QString reportType = detectReportType(options);
    const QString generatedAt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    const QString reportTitle = reportTitleForType(reportType);
    const QString reportDescription = reportDescriptionForType(reportType);
    const QString htmlContent = buildOverviewHtml(reportTitle,
                                                  reportDescription,
                                                  generatedAt,
                                                  options.currentTaskState,
                                                  copiedFiles,
                                                  options.notes,
                                                  options.criteriaSummary,
                                                  options.metricSummary,
                                                  options.templateSettings,
                                                  bundleDirectory);

    const QString htmlPath = QDir(bundleDirectory).filePath(QObject::tr("report_overview.html"));
    if (!writeTextFile(htmlPath, htmlContent, errorMessage)) {
        return false;
    }

    QString excelContent;
    QTextStream excelStream(&excelContent);
    excelStream << "<?xml version=\"1.0\"?>";
    excelStream << "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\" "
                   "xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">";
    excelStream << "<Worksheet ss:Name=\"" << htmlEscape(QObject::tr("报告总览")) << "\"><Table>";
    auto writeExcelRow = [&excelStream](const QStringList& cells) {
        excelStream << "<Row>";
        for (const QString& cell : cells) {
            excelStream << "<Cell><Data ss:Type=\"String\">" << htmlEscape(cell) << "</Data></Cell>";
        }
        excelStream << "</Row>";
    };
    writeExcelRow({QObject::tr("报告标题"),
                   options.templateSettings.reportTitle.isEmpty() ? reportTitle : options.templateSettings.reportTitle});
    writeExcelRow({QObject::tr("报告类型"), reportType});
    writeExcelRow({QObject::tr("生成时间"), generatedAt});
    writeExcelRow({QObject::tr("客户名称"), options.templateSettings.customerName});
    writeExcelRow({QObject::tr("任务状态"), options.currentTaskState.isEmpty() ? QObject::tr("空闲") : options.currentTaskState});
    writeExcelRow({QObject::tr("文件数量"), QString::number(copiedFiles.size())});
    writeExcelRow({QObject::tr("页脚说明"), options.templateSettings.footerText});
    excelStream << "</Table></Worksheet>";
    if (!options.criteriaSummary.isEmpty()) {
        excelStream << "<Worksheet ss:Name=\"" << htmlEscape(QObject::tr("筛选条件")) << "\"><Table>";
        writeExcelRow({QObject::tr("筛选条件")});
        for (const QString& item : options.criteriaSummary) {
            writeExcelRow({item});
        }
        excelStream << "</Table></Worksheet>";
    }
    if (!options.metricSummary.isEmpty()) {
        excelStream << "<Worksheet ss:Name=\"" << htmlEscape(QObject::tr("统计摘要")) << "\"><Table>";
        writeExcelRow({QObject::tr("统计摘要")});
        for (const QString& item : options.metricSummary) {
            writeExcelRow({item});
        }
        excelStream << "</Table></Worksheet>";
    }
    excelStream << "<Worksheet ss:Name=\"" << htmlEscape(QObject::tr("导出文件")) << "\"><Table>";
    writeExcelRow({QObject::tr("文件名"), QObject::tr("类型"), QObject::tr("绝对路径")});
    for (const QString& filePath : copiedFiles) {
        const QFileInfo fileInfo(filePath);
        writeExcelRow({fileInfo.fileName(), fileInfo.suffix().toUpper(), fileInfo.absoluteFilePath()});
    }
    excelStream << "</Table></Worksheet>";
    if (!options.notes.isEmpty()) {
        excelStream << "<Worksheet ss:Name=\"" << htmlEscape(QObject::tr("备注")) << "\"><Table>";
        writeExcelRow({QObject::tr("备注")});
        for (const QString& note : options.notes) {
            writeExcelRow({note});
        }
        excelStream << "</Table></Worksheet>";
    }
    excelStream << "</Workbook>";

    const QString excelPath = QDir(bundleDirectory).filePath(QObject::tr("report_overview.xls"));
    if (!writeTextFile(excelPath, excelContent, errorMessage)) {
        return false;
    }

    QPdfWriter pdfWriter(QDir(bundleDirectory).filePath(QObject::tr("report_overview.pdf")));
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageMargins(QMarginsF(16, 16, 16, 16));
    QTextDocument pdfDocument;
    pdfDocument.setDefaultFont(QFont(QObject::tr("Microsoft YaHei UI"), 10));
    pdfDocument.setHtml(htmlContent);
    pdfDocument.print(&pdfWriter);

    if (outputPath != nullptr) {
        *outputPath = bundleDirectory;
    }
    Logger::info("Created report bundle: " + bundleDirectory);
    return true;
}

}  // namespace LaserSpc::Infrastructure

