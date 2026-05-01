#pragma once

#include "domain/Models.h"
#include "infrastructure/DatabaseConnection.h"
#include "ui/common/UiTextCatalog.h"

namespace LaserSpc::Infrastructure {

enum class AppLanguage {
    Chinese,
    English
};

enum class ThemeStyle {
    Night,
    Ocean,
    Graphite,
    Sand,
    Forest,
    Ember,
    Aurora
};

struct ReportTemplateSettings {
    QString reportTitle = LaserSpc::Ui::TextCatalog::defaultReportTitle();
    QString customerName;
    QString footerText = LaserSpc::Ui::TextCatalog::defaultReportFooter();
    QString logoPath;
};

struct MesSettings {
    bool enabled = false;
    QString endpointUrl;
    QString siteCode;
    QString stationCode;
    QString userName;
    int timeoutSeconds = 8;
};

struct UiSettings {
    AppLanguage language = AppLanguage::Chinese;
    ThemeStyle themeStyle = ThemeStyle::Night;
};

struct DataCleanupSettings {
    int productionRetentionDays = 30;
};

struct AppSettings {
    bool useMySql = true;
    bool allowMockFallback = false;
    bool systemSettingsEnabled = true;
    bool exportReportEnabled = true;
    int defaultQueryDays = 7;
    bool autoRefreshEnabled = false;
    int autoRefreshIntervalSeconds = 60;
    QString exportDirectory;
    QString pointDetailDirectory;
    DatabaseSettings database;
    MesSettings mes;
    UiSettings ui;
    DataCleanupSettings cleanup;
    ReportTemplateSettings reportTemplate;
};

QString toConfigValue(AppLanguage language);
AppLanguage appLanguageFromConfig(const QString& value);
QString toConfigValue(ThemeStyle themeStyle);
ThemeStyle themeStyleFromConfig(const QString& value);
QString defaultMySqlConnectOptions();

class AppConfigService {
public:
    AppConfigService();

    QString applicationName() const;
    QString dataSourceMode() const;
    LaserSpc::Domain::FilterCriteria defaultFilter() const;
    DatabaseSettings mysqlSettings() const;
    bool preferMySql() const;
    AppSettings settings() const;
    ReportTemplateSettings reportTemplateSettings() const;
    bool saveSettings(const AppSettings& settings, QString* errorMessage = nullptr) const;
    QString configFilePath() const;
    static LaserSpc::Domain::FilterCriteria buildDefaultFilter(const AppSettings& settings);

private:
    void loadDefaults();
    void loadFromFile();
    void applyEnvironmentOverrides();
    void rebuildDefaultFilter();

    LaserSpc::Domain::FilterCriteria m_defaultFilter;
    AppSettings m_settings;
};

}  // namespace LaserSpc::Infrastructure
