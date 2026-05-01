#include "infrastructure/AppConfigService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QtGlobal>

#include "ui/common/UiTextCatalog.h"

namespace LaserSpc::Infrastructure {

namespace {

void configureIniUtf8(QSettings& settings) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#else
    Q_UNUSED(settings);
#endif
}

QString boolToIniValue(bool value) {
    return value ? QStringLiteral("1") : QStringLiteral("0");
}

}  // namespace

QString toConfigValue(AppLanguage language) {
    switch (language) {
        case AppLanguage::English: return QStringLiteral("en_US");
        case AppLanguage::Chinese:
        default: return QStringLiteral("zh_CN");
    }
}

AppLanguage appLanguageFromConfig(const QString& value) {
    return value.trimmed().toLower().startsWith(QStringLiteral("en")) ? AppLanguage::English : AppLanguage::Chinese;
}

QString toConfigValue(ThemeStyle themeStyle) {
    switch (themeStyle) {
        case ThemeStyle::Night: return QStringLiteral("night");
        case ThemeStyle::Graphite: return QStringLiteral("graphite");
        case ThemeStyle::Sand: return QStringLiteral("sand");
        case ThemeStyle::Forest: return QStringLiteral("forest");
        case ThemeStyle::Ember: return QStringLiteral("ember");
        case ThemeStyle::Aurora: return QStringLiteral("aurora");
        case ThemeStyle::Ocean:
        default: return QStringLiteral("ocean");
    }
}

QString defaultMySqlConnectOptions() {
    return QStringLiteral("MYSQL_OPT_SSL_ENFORCE=0;MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0");
}

ThemeStyle themeStyleFromConfig(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("night")) {
        return ThemeStyle::Night;
    }
    if (normalized == QStringLiteral("graphite")) {
        return ThemeStyle::Graphite;
    }
    if (normalized == QStringLiteral("sand")) {
        return ThemeStyle::Sand;
    }
    if (normalized == QStringLiteral("forest")) {
        return ThemeStyle::Forest;
    }
    if (normalized == QStringLiteral("ember")) {
        return ThemeStyle::Ember;
    }
    if (normalized == QStringLiteral("aurora")) {
        return ThemeStyle::Aurora;
    }
    return ThemeStyle::Night;
}

AppConfigService::AppConfigService() {
    loadDefaults();
    loadFromFile();
    applyEnvironmentOverrides();
    rebuildDefaultFilter();
}

QString AppConfigService::applicationName() const {
    return QStringLiteral("LaserSpc");
}

QString AppConfigService::dataSourceMode() const {
    return preferMySql()
               ? QString("MySQL %1:%2/%3")
                     .arg(m_settings.database.host)
                     .arg(m_settings.database.port)
                     .arg(m_settings.database.databaseName)
               : QStringLiteral("Mock Repository");
}

LaserSpc::Domain::FilterCriteria AppConfigService::defaultFilter() const {
    return m_defaultFilter;
}

DatabaseSettings AppConfigService::mysqlSettings() const {
    return m_settings.database;
}

bool AppConfigService::preferMySql() const {
    return m_settings.useMySql;
}

AppSettings AppConfigService::settings() const {
    return m_settings;
}

ReportTemplateSettings AppConfigService::reportTemplateSettings() const {
    return m_settings.reportTemplate;
}

bool AppConfigService::saveSettings(const AppSettings& settings, QString* errorMessage) const {
    QFileInfo info(configFilePath());
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to create config directory.");
        }
        return false;
    }

    QSettings fileSettings(configFilePath(), QSettings::IniFormat);
    configureIniUtf8(fileSettings);
    fileSettings.clear();
    fileSettings.setValue("general/use_mysql", boolToIniValue(settings.useMySql));
    fileSettings.setValue("general/allow_mock_fallback", boolToIniValue(settings.allowMockFallback));
    fileSettings.setValue("general/system_settings_enabled", boolToIniValue(settings.systemSettingsEnabled));
    fileSettings.setValue("general/export_report_enabled", boolToIniValue(settings.exportReportEnabled));
    fileSettings.setValue("general/default_query_days", settings.defaultQueryDays);
    fileSettings.setValue("general/auto_refresh_enabled", boolToIniValue(settings.autoRefreshEnabled));
    fileSettings.setValue("general/auto_refresh_interval_seconds", settings.autoRefreshIntervalSeconds);
    fileSettings.setValue("general/export_directory", settings.exportDirectory);
    fileSettings.setValue("general/point_detail_directory", settings.pointDetailDirectory);
    fileSettings.setValue("cleanup/production_retention_days", settings.cleanup.productionRetentionDays);
    fileSettings.setValue("ui/language", toConfigValue(settings.ui.language));
    fileSettings.setValue("ui/theme_style", toConfigValue(settings.ui.themeStyle));
    fileSettings.setValue("mysql/host", settings.database.host);
    fileSettings.setValue("mysql/port", settings.database.port);
    fileSettings.setValue("mysql/database_name", settings.database.databaseName);
    fileSettings.setValue("mysql/user_name", settings.database.userName);
    fileSettings.setValue("mysql/password", settings.database.password);
    fileSettings.setValue("mysql/connect_options", settings.database.connectOptions);
    fileSettings.setValue("mysql/connect_timeout_seconds", settings.database.connectTimeoutSeconds);
    fileSettings.setValue("mysql/read_timeout_seconds", settings.database.readTimeoutSeconds);
    fileSettings.setValue("mes/enabled", boolToIniValue(settings.mes.enabled));
    fileSettings.setValue("mes/endpoint_url", settings.mes.endpointUrl);
    fileSettings.setValue("mes/site_code", settings.mes.siteCode);
    fileSettings.setValue("mes/station_code", settings.mes.stationCode);
    fileSettings.setValue("mes/user_name", settings.mes.userName);
    fileSettings.setValue("mes/timeout_seconds", settings.mes.timeoutSeconds);
    fileSettings.setValue("report/title", settings.reportTemplate.reportTitle);
    fileSettings.setValue("report/customer_name", settings.reportTemplate.customerName);
    fileSettings.setValue("report/footer_text", settings.reportTemplate.footerText);
    fileSettings.setValue("report/logo_path", settings.reportTemplate.logoPath);
    fileSettings.sync();

    if (fileSettings.status() != QSettings::NoError) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to persist config file.");
        }
        return false;
    }

    const QFileInfo writtenFile(configFilePath());
    if (!writtenFile.exists() || writtenFile.size() <= 0) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to write config file.");
        }
        return false;
    }
    return true;
}

QString AppConfigService::configFilePath() const {
    const QString overridePath = qEnvironmentVariable("LASERSPC_CONFIG_FILE");
    if (!overridePath.isEmpty()) {
        return overridePath;
    }

    QDir dir(QCoreApplication::applicationDirPath());
    if (!dir.exists("config")) {
        dir.mkpath("config");
    }
    return dir.filePath("config/laserspc.ini");
}

LaserSpc::Domain::FilterCriteria AppConfigService::buildDefaultFilter(const AppSettings& settings) {
    LaserSpc::Domain::FilterCriteria filter;
    filter.beginTime = QDateTime::currentDateTime().addDays(-qMax(1, settings.defaultQueryDays));
    filter.endTime = QDateTime::currentDateTime();
    filter.lineName = LaserSpc::Ui::TextCatalog::allSelection();
    filter.programName = LaserSpc::Ui::TextCatalog::allSelection();
    filter.deviceName = LaserSpc::Ui::TextCatalog::allSelection();
    filter.result = LaserSpc::Ui::TextCatalog::allSelection();
    filter.keyword.clear();
    return filter;
}

void AppConfigService::loadDefaults() {
    m_settings.useMySql = true;
    m_settings.allowMockFallback = false;
    m_settings.systemSettingsEnabled = true;
    m_settings.exportReportEnabled = true;
    m_settings.defaultQueryDays = 7;
    m_settings.autoRefreshEnabled = false;
    m_settings.autoRefreshIntervalSeconds = 60;
    m_settings.exportDirectory.clear();
    m_settings.pointDetailDirectory.clear();
    m_settings.cleanup.productionRetentionDays = 30;
    m_settings.ui.language = AppLanguage::Chinese;
    m_settings.ui.themeStyle = ThemeStyle::Night;
    m_settings.database.host = QStringLiteral("127.0.0.1");
    m_settings.database.port = 3306;
    m_settings.database.databaseName = QStringLiteral("laser_spc");
    m_settings.database.userName = QStringLiteral("laserspc");
    m_settings.database.password = QStringLiteral("LaserSpc#2026");
    m_settings.database.connectOptions = defaultMySqlConnectOptions();
    m_settings.database.connectTimeoutSeconds = 5;
    m_settings.database.readTimeoutSeconds = 15;
    m_settings.mes.enabled = false;
    m_settings.mes.endpointUrl.clear();
    m_settings.mes.siteCode.clear();
    m_settings.mes.stationCode.clear();
    m_settings.mes.userName.clear();
    m_settings.mes.timeoutSeconds = 8;
    m_settings.reportTemplate.reportTitle = LaserSpc::Ui::TextCatalog::defaultReportTitle();
    m_settings.reportTemplate.customerName.clear();
    m_settings.reportTemplate.footerText = LaserSpc::Ui::TextCatalog::defaultReportFooter();
    m_settings.reportTemplate.logoPath.clear();
}

void AppConfigService::loadFromFile() {
    QFileInfo info(configFilePath());
    if (!info.exists()) {
        return;
    }

    QSettings fileSettings(configFilePath(), QSettings::IniFormat);
    configureIniUtf8(fileSettings);
    m_settings.useMySql = fileSettings.value("general/use_mysql", m_settings.useMySql).toBool();
    m_settings.allowMockFallback =
        fileSettings.value("general/allow_mock_fallback", m_settings.allowMockFallback).toBool();
    m_settings.systemSettingsEnabled =
        fileSettings.value("general/system_settings_enabled", m_settings.systemSettingsEnabled).toBool();
    m_settings.exportReportEnabled =
        fileSettings.value("general/export_report_enabled", m_settings.exportReportEnabled).toBool();
    m_settings.defaultQueryDays = fileSettings.value("general/default_query_days", m_settings.defaultQueryDays).toInt();
    m_settings.autoRefreshEnabled =
        fileSettings.value("general/auto_refresh_enabled", m_settings.autoRefreshEnabled).toBool();
    m_settings.autoRefreshIntervalSeconds =
        fileSettings.value("general/auto_refresh_interval_seconds", m_settings.autoRefreshIntervalSeconds).toInt();
    m_settings.exportDirectory =
        fileSettings.value("general/export_directory", m_settings.exportDirectory).toString().trimmed();
    m_settings.pointDetailDirectory =
        fileSettings.value("general/point_detail_directory", m_settings.pointDetailDirectory).toString().trimmed();
    m_settings.cleanup.productionRetentionDays =
        fileSettings.value("cleanup/production_retention_days", m_settings.cleanup.productionRetentionDays).toInt();
    m_settings.ui.language = appLanguageFromConfig(fileSettings.value("ui/language", QStringLiteral("zh_CN")).toString());
    m_settings.ui.themeStyle =
        themeStyleFromConfig(fileSettings.value("ui/theme_style", QStringLiteral("night")).toString());
    if (m_settings.defaultQueryDays <= 0) {
        m_settings.defaultQueryDays = 7;
    }
    if (m_settings.autoRefreshIntervalSeconds <= 0) {
        m_settings.autoRefreshIntervalSeconds = 60;
    }
    if (m_settings.cleanup.productionRetentionDays <= 0) {
        m_settings.cleanup.productionRetentionDays = 30;
    }

    m_settings.database.host = fileSettings.value("mysql/host", m_settings.database.host).toString();
    m_settings.database.port = fileSettings.value("mysql/port", m_settings.database.port).toInt();
    if (m_settings.database.port <= 0) {
        m_settings.database.port = 3306;
    }
    m_settings.database.databaseName =
        fileSettings.value("mysql/database_name", m_settings.database.databaseName).toString();
    m_settings.database.userName = fileSettings.value("mysql/user_name", m_settings.database.userName).toString();
    m_settings.database.password = fileSettings.value("mysql/password", m_settings.database.password).toString();
    m_settings.database.connectOptions =
        fileSettings.value("mysql/connect_options", m_settings.database.connectOptions).toString().trimmed();
    m_settings.database.connectTimeoutSeconds =
        fileSettings.value("mysql/connect_timeout_seconds", m_settings.database.connectTimeoutSeconds).toInt();
    if (m_settings.database.connectTimeoutSeconds <= 0) {
        m_settings.database.connectTimeoutSeconds = 5;
    }
    m_settings.database.readTimeoutSeconds =
        fileSettings.value("mysql/read_timeout_seconds", m_settings.database.readTimeoutSeconds).toInt();
    if (m_settings.database.readTimeoutSeconds <= 0) {
        m_settings.database.readTimeoutSeconds = 15;
    }
    if (m_settings.database.connectOptions.trimmed().isEmpty()) {
        m_settings.database.connectOptions = defaultMySqlConnectOptions();
    }

    m_settings.mes.enabled = fileSettings.value("mes/enabled", m_settings.mes.enabled).toBool();
    m_settings.mes.endpointUrl = fileSettings.value("mes/endpoint_url", m_settings.mes.endpointUrl).toString();
    m_settings.mes.siteCode = fileSettings.value("mes/site_code", m_settings.mes.siteCode).toString();
    m_settings.mes.stationCode = fileSettings.value("mes/station_code", m_settings.mes.stationCode).toString();
    m_settings.mes.userName = fileSettings.value("mes/user_name", m_settings.mes.userName).toString();
    m_settings.mes.timeoutSeconds = fileSettings.value("mes/timeout_seconds", m_settings.mes.timeoutSeconds).toInt();
    if (m_settings.mes.timeoutSeconds <= 0) {
        m_settings.mes.timeoutSeconds = 8;
    }

    m_settings.reportTemplate.reportTitle =
        fileSettings.value("report/title", m_settings.reportTemplate.reportTitle).toString();
    m_settings.reportTemplate.customerName =
        fileSettings.value("report/customer_name", m_settings.reportTemplate.customerName).toString();
    m_settings.reportTemplate.footerText =
        fileSettings.value("report/footer_text", m_settings.reportTemplate.footerText).toString();
    m_settings.reportTemplate.logoPath =
        fileSettings.value("report/logo_path", m_settings.reportTemplate.logoPath).toString();
}

void AppConfigService::applyEnvironmentOverrides() {
    const QString useMySqlValue = qEnvironmentVariable("LASERSPC_USE_MYSQL");
    if (!useMySqlValue.isEmpty()) {
        const QString value = useMySqlValue.trimmed().toLower();
        m_settings.useMySql = value != "0" && value != "false" && value != "off";
    }

    const QString allowMockFallbackValue = qEnvironmentVariable("LASERSPC_ALLOW_MOCK_FALLBACK");
    if (!allowMockFallbackValue.isEmpty()) {
        const QString value = allowMockFallbackValue.trimmed().toLower();
        m_settings.allowMockFallback = value != "0" && value != "false" && value != "off";
    }

    const QString systemSettingsEnabledValue = qEnvironmentVariable("LASERSPC_SYSTEM_SETTINGS_ENABLED");
    if (!systemSettingsEnabledValue.isEmpty()) {
        const QString value = systemSettingsEnabledValue.trimmed().toLower();
        m_settings.systemSettingsEnabled = value != "0" && value != "false" && value != "off";
    }

    const QString exportReportEnabledValue = qEnvironmentVariable("LASERSPC_EXPORT_REPORT_ENABLED");
    if (!exportReportEnabledValue.isEmpty()) {
        const QString value = exportReportEnabledValue.trimmed().toLower();
        m_settings.exportReportEnabled = value != "0" && value != "false" && value != "off";
    }

    const QString host = qEnvironmentVariable("LASERSPC_DB_HOST");
    if (!host.isEmpty()) {
        m_settings.database.host = host;
    }

    const int port = qEnvironmentVariableIntValue("LASERSPC_DB_PORT");
    if (port > 0) {
        m_settings.database.port = port;
    }

    const QString databaseName = qEnvironmentVariable("LASERSPC_DB_NAME");
    if (!databaseName.isEmpty()) {
        m_settings.database.databaseName = databaseName;
    }

    const QString userName = qEnvironmentVariable("LASERSPC_DB_USER");
    if (!userName.isEmpty()) {
        m_settings.database.userName = userName;
    }

    const QString password = qEnvironmentVariable("LASERSPC_DB_PASSWORD");
    if (!password.isEmpty()) {
        m_settings.database.password = password;
    }

    const QString connectOptions = qEnvironmentVariable("LASERSPC_DB_CONNECT_OPTIONS");
    if (!connectOptions.isEmpty()) {
        m_settings.database.connectOptions = connectOptions;
    }

    const int connectTimeout = qEnvironmentVariableIntValue("LASERSPC_DB_CONNECT_TIMEOUT_SECONDS");
    if (connectTimeout > 0) {
        m_settings.database.connectTimeoutSeconds = connectTimeout;
    }

    const int readTimeout = qEnvironmentVariableIntValue("LASERSPC_DB_READ_TIMEOUT_SECONDS");
    if (readTimeout > 0) {
        m_settings.database.readTimeoutSeconds = readTimeout;
    }

    const QString exportDirectory = qEnvironmentVariable("LASERSPC_EXPORT_DIR");
    if (!exportDirectory.isEmpty()) {
        m_settings.exportDirectory = exportDirectory.trimmed();
    }

    const QString pointDetailDirectory = qEnvironmentVariable("LASERSPC_POINT_DETAIL_DIR");
    if (!pointDetailDirectory.isEmpty()) {
        m_settings.pointDetailDirectory = pointDetailDirectory.trimmed();
    }

    const QString reportTitle = qEnvironmentVariable("LASERSPC_REPORT_TITLE");
    if (!reportTitle.isEmpty()) {
        m_settings.reportTemplate.reportTitle = reportTitle;
    }

    const QString customerName = qEnvironmentVariable("LASERSPC_REPORT_CUSTOMER");
    if (!customerName.isEmpty()) {
        m_settings.reportTemplate.customerName = customerName;
    }

    const QString footerText = qEnvironmentVariable("LASERSPC_REPORT_FOOTER");
    if (!footerText.isEmpty()) {
        m_settings.reportTemplate.footerText = footerText;
    }

    const QString logoPath = qEnvironmentVariable("LASERSPC_REPORT_LOGO");
    if (!logoPath.isEmpty()) {
        m_settings.reportTemplate.logoPath = logoPath;
    }

    const int retentionDays = qEnvironmentVariableIntValue("LASERSPC_PRODUCTION_RETENTION_DAYS");
    if (retentionDays > 0) {
        m_settings.cleanup.productionRetentionDays = retentionDays;
    }
}

void AppConfigService::rebuildDefaultFilter() {
    m_defaultFilter = buildDefaultFilter(m_settings);
}

}  // namespace LaserSpc::Infrastructure
