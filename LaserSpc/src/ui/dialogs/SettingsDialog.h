#pragma once

#include <QDialog>
#include <QFutureWatcher>

#include "infrastructure/AppConfigService.h"
#include "infrastructure/DataCleanupService.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QLabel;
class QTabWidget;
class QPushButton;
class QWidget;

namespace LaserSpc::Ui {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const LaserSpc::Infrastructure::AppSettings& settings, QWidget* parent = nullptr);

    LaserSpc::Infrastructure::AppSettings settings() const;

signals:
    void dataCleanupFinished(const QString& message);

private:
    enum class CleanupMode {
        None,
        Seed,
        Production
    };

    QString textFor(const char* chinese, const char* english) const;

    void setupUi();
    void loadSettings(const LaserSpc::Infrastructure::AppSettings& settings);
    void updatePointDetailPathHint();
    void updateFieldState();
    void runSeedCleanup();
    void runProductionCleanup();
    void startCleanupTask(CleanupMode mode,
                          const LaserSpc::Infrastructure::DatabaseSettings& databaseSettings,
                          int retentionDays = 0);
    void finishCleanupTask();

    QTabWidget* m_tabWidget = nullptr;
    QCheckBox* m_useMySqlCheckBox = nullptr;
    QLineEdit* m_hostEdit = nullptr;
    QSpinBox* m_portSpinBox = nullptr;
    QLineEdit* m_databaseNameEdit = nullptr;
    QLineEdit* m_userNameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QSpinBox* m_connectTimeoutSpinBox = nullptr;
    QSpinBox* m_readTimeoutSpinBox = nullptr;
    QSpinBox* m_defaultQueryDaysSpinBox = nullptr;
    QCheckBox* m_autoRefreshCheckBox = nullptr;
    QSpinBox* m_autoRefreshIntervalSpinBox = nullptr;
    QWidget* m_pointDetailDirectoryRow = nullptr;
    QLineEdit* m_pointDetailDirectoryEdit = nullptr;
    QPushButton* m_pointDetailBrowseButton = nullptr;
    QLabel* m_pointDetailDefaultPathLabel = nullptr;
    QSpinBox* m_cleanupRetentionDaysSpinBox = nullptr;
    QPushButton* m_cleanupSeedButton = nullptr;
    QPushButton* m_cleanupProductionButton = nullptr;
    QLabel* m_cleanupStatusLabel = nullptr;
    QLineEdit* m_reportTitleEdit = nullptr;
    QLineEdit* m_customerNameEdit = nullptr;
    QLineEdit* m_footerTextEdit = nullptr;
    QLineEdit* m_logoPathEdit = nullptr;
    QComboBox* m_languageCombo = nullptr;
    QComboBox* m_themeCombo = nullptr;
    QString m_connectOptions;
    bool m_english = false;
    CleanupMode m_cleanupMode = CleanupMode::None;
    QFutureWatcher<LaserSpc::Infrastructure::DataCleanupResult> m_cleanupWatcher;
};

}  // namespace LaserSpc::Ui
