#include "infrastructure/RuntimeDiagnostics.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QTextStream>

namespace LaserSpc::Infrastructure {

namespace {

QString windowsFontDirectory() {
#ifdef Q_OS_WIN
    const QString windir = qEnvironmentVariable("WINDIR", QObject::tr("C:/Windows"));
    return QDir(windir).filePath(QObject::tr("Fonts"));
#else
    return QString();
#endif
}

QString stagedWindowsFontDirectory() {
#ifdef Q_OS_WIN
    const QString sourceDirectory = windowsFontDirectory();
    if (sourceDirectory.isEmpty() || !QDir(sourceDirectory).exists()) {
        return QString();
    }

    const QString targetDirectory = QDir::temp().filePath(QObject::tr("LaserSpcQtFonts"));
    QDir().mkpath(targetDirectory);

    const QStringList candidates = {
        QObject::tr("segoeui.ttf"),
        QObject::tr("arial.ttf"),
        QObject::tr("tahoma.ttf"),
        QObject::tr("msyh.ttc"),
        QObject::tr("msyh.ttf")
    };

    int copiedCount = 0;
    for (const QString& fileName : candidates) {
        const QString sourcePath = QDir(sourceDirectory).filePath(fileName);
        if (!QFileInfo::exists(sourcePath)) {
            continue;
        }

        const QString targetPath = QDir(targetDirectory).filePath(fileName);
        if (!QFileInfo::exists(targetPath)) {
            QFile::remove(targetPath);
            if (!QFile::copy(sourcePath, targetPath)) {
                continue;
            }
        }
        ++copiedCount;
    }

    return copiedCount > 0 ? targetDirectory : QString();
#else
    return QString();
#endif
}

QString effectiveFontDirectory(const QString& overridePath) {
    if (!overridePath.isEmpty()) {
        return overridePath;
    }

    const QString envFontDir = qEnvironmentVariable("QT_QPA_FONTDIR");
    if (!envFontDir.isEmpty()) {
        return envFontDir;
    }

    const QString qtFontDir = RuntimeDiagnostics::qtFontDirectory();
    if (QDir(qtFontDir).exists()) {
        return qtFontDir;
    }

    const QString fallbackFontDir = stagedWindowsFontDirectory();
    if (!fallbackFontDir.isEmpty() && QDir(fallbackFontDir).exists()) {
        return fallbackFontDir;
    }

    return qtFontDir;
}

QString effectivePluginDirectory(const QString& overridePath) {
    if (!overridePath.isEmpty()) {
        return overridePath;
    }

    const QString envPluginPath = qEnvironmentVariable("QT_PLUGIN_PATH");
    if (!envPluginPath.isEmpty()) {
        const QStringList paths = envPluginPath.split(';', Qt::SkipEmptyParts);
        for (const QString& path : paths) {
            if (QDir(path).exists()) {
                return path;
            }
        }
    }

    return RuntimeDiagnostics::qtPluginDirectory();
}

}  // namespace

void RuntimeDiagnostics::prepareQtRuntime(const QString& fontDirectoryOverride,
                                          const QString& pluginDirectoryOverride) {
    const QString fontDirectory = effectiveFontDirectory(fontDirectoryOverride);
    if (!fontDirectory.isEmpty() && QDir(fontDirectory).exists() && qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR")) {
        qputenv("QT_QPA_FONTDIR", fontDirectory.toUtf8());
    }

    const QString pluginDirectory = effectivePluginDirectory(pluginDirectoryOverride);
    if (!pluginDirectory.isEmpty() && QDir(pluginDirectory).exists() && qEnvironmentVariableIsEmpty("QT_PLUGIN_PATH")) {
        qputenv("QT_PLUGIN_PATH", pluginDirectory.toUtf8());
    }
}

QString RuntimeDiagnostics::qtFontDirectory() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QLibraryInfo::path(QLibraryInfo::LibrariesPath) + "/fonts";
#else
    return QLibraryInfo::location(QLibraryInfo::LibrariesPath) + "/fonts";
#endif
}

QString RuntimeDiagnostics::qtPluginDirectory() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QLibraryInfo::path(QLibraryInfo::PluginsPath);
#else
    return QLibraryInfo::location(QLibraryInfo::PluginsPath);
#endif
}

RuntimeDiagnosticsSnapshot RuntimeDiagnostics::collectSnapshot(const QString& configFilePath,
                                                              const QString& exportDirectory,
                                                              const QString& fontDirectoryOverride,
                                                              const QString& pluginDirectoryOverride,
                                                              const QStringList& sqlDriversOverride) {
    RuntimeDiagnosticsSnapshot snapshot;
    snapshot.configFilePath = configFilePath;
    snapshot.exportDirectory = exportDirectory;
    snapshot.qtFontDirectory = effectiveFontDirectory(fontDirectoryOverride);
    snapshot.qtPluginDirectory = effectivePluginDirectory(pluginDirectoryOverride);
    snapshot.sqlDrivers = sqlDriversOverride.isEmpty() ? QSqlDatabase::drivers() : sqlDriversOverride;
    snapshot.warnings = startupWarnings(fontDirectoryOverride, pluginDirectoryOverride, snapshot.sqlDrivers);
    return snapshot;
}

bool RuntimeDiagnostics::exportSnapshotReport(const RuntimeDiagnosticsSnapshot& snapshot,
                                              QString* outputPath,
                                              QString* errorMessage) {
    const QString reportDirectory = snapshot.exportDirectory.isEmpty()
                                        ? QDir::temp().filePath(QObject::tr("LaserSpcDiagnostics"))
                                        : snapshot.exportDirectory;
    QDir().mkpath(reportDirectory);
    const QString filePath =
        QDir(reportDirectory).filePath(QObject::tr("laser_spc_runtime_diagnostics_%1.txt")
                                           .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QTextStream stream(&file);
    stream << "LaserSpc Runtime Diagnostics\n";
    stream << "GeneratedAt=" << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    stream << "ConfigFile=" << snapshot.configFilePath << "\n";
    stream << "ExportDirectory=" << snapshot.exportDirectory << "\n";
    stream << "QtFontDirectory=" << snapshot.qtFontDirectory << "\n";
    stream << "QtPluginDirectory=" << snapshot.qtPluginDirectory << "\n";
    stream << "SqlDrivers=" << snapshot.sqlDrivers.join(", ") << "\n";
    stream << "Warnings=" << snapshot.warnings.size() << "\n";
    for (const QString& warning : snapshot.warnings) {
        stream << "- " << warning << "\n";
    }

    if (!file.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    if (outputPath != nullptr) {
        *outputPath = filePath;
    }
    return true;
}

QStringList RuntimeDiagnostics::startupWarnings(const QString& fontDirectoryOverride,
                                                const QString& pluginDirectoryOverride,
                                                const QStringList& sqlDriversOverride) {
    QStringList warnings;
    const QString fontDirectory = effectiveFontDirectory(fontDirectoryOverride);
    const QString pluginDirectory = effectivePluginDirectory(pluginDirectoryOverride);
    const QStringList sqlDrivers = sqlDriversOverride.isEmpty() ? QSqlDatabase::drivers() : sqlDriversOverride;

    if (!fontDirectory.isEmpty() && !QDir(fontDirectory).exists()) {
        warnings.append(QString("Qt font directory missing: %1").arg(fontDirectory));
    }
    if (!pluginDirectory.isEmpty() && !QDir(pluginDirectory).exists()) {
        warnings.append(QString("Qt plugin directory missing: %1").arg(pluginDirectory));
    }
    if (!sqlDrivers.contains("QMYSQL")) {
        warnings.append(QObject::tr("QMYSQL driver is unavailable; MySQL mode may fail."));
    }

    return warnings;
}

}  // namespace LaserSpc::Infrastructure
