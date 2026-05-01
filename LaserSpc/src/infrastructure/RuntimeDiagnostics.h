#pragma once

#include <QString>
#include <QStringList>

namespace LaserSpc::Infrastructure {

struct RuntimeDiagnosticsSnapshot {
    QString configFilePath;
    QString exportDirectory;
    QString qtFontDirectory;
    QString qtPluginDirectory;
    QStringList sqlDrivers;
    QStringList warnings;
};

class RuntimeDiagnostics {
public:
    static void prepareQtRuntime(const QString& fontDirectoryOverride = QString(),
                                 const QString& pluginDirectoryOverride = QString());
    static QString qtFontDirectory();
    static QString qtPluginDirectory();
    static RuntimeDiagnosticsSnapshot collectSnapshot(const QString& configFilePath = QString(),
                                                     const QString& exportDirectory = QString(),
                                                     const QString& fontDirectoryOverride = QString(),
                                                     const QString& pluginDirectoryOverride = QString(),
                                                     const QStringList& sqlDriversOverride = QStringList());
    static bool exportSnapshotReport(const RuntimeDiagnosticsSnapshot& snapshot,
                                     QString* outputPath,
                                     QString* errorMessage);
    static QStringList startupWarnings(const QString& fontDirectoryOverride = QString(),
                                       const QString& pluginDirectoryOverride = QString(),
                                       const QStringList& sqlDriversOverride = QStringList());
};

}  // namespace LaserSpc::Infrastructure
