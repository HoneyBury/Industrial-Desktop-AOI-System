#include "ui/dialogs/SettingsDialog.h"

#include <QCoreApplication>
#include <QDir>
#include <QVariant>

#include <QtConcurrent>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "infrastructure/Logger.h"
#include "ui/common/UiTheme.h"

namespace LaserSpc::Ui {

SettingsDialog::SettingsDialog(const LaserSpc::Infrastructure::AppSettings& settings, QWidget* parent)
    : QDialog(parent), m_english(settings.ui.language == LaserSpc::Infrastructure::AppLanguage::English) {
    setupUi();
    loadSettings(settings);
    updateFieldState();
}

QString SettingsDialog::textFor(const char* chinese, const char* english) const {
    return m_english ? tr(english) : tr(chinese);
}

void SettingsDialog::setupUi() {
    setWindowTitle(textFor("系统设置", "System Settings"));
    resize(700, 560);
    UiTheme::applyPanel(this);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 16);
    rootLayout->setSpacing(14);

    auto* titleLabel = new QLabel(textFor("系统设置", "System Settings"), this);
    titleLabel->setProperty("dialogTitle", QVariant(true));

    auto* infoLabel = new QLabel(
        textFor("保存后会立即更新默认查询范围，并按新配置重建数据源连接。数据清理会直接作用于当前数据库，请谨慎执行。",
                "Changes take effect immediately after saving. Cleanup actions operate directly on the current database."),
        this);
    infoLabel->setProperty("hint", QVariant(true));
    infoLabel->setWordWrap(true);

    m_tabWidget = new QTabWidget(this);

    auto* databaseTab = new QWidget(this);
    auto* databaseLayout = new QVBoxLayout(databaseTab);
    databaseLayout->setContentsMargins(14, 14, 14, 14);
    auto* databaseCard = new QFrame(databaseTab);
    UiTheme::applyPanel(databaseCard);
    auto* databaseCardLayout = new QVBoxLayout(databaseCard);
    databaseCardLayout->setContentsMargins(16, 16, 16, 16);
    auto* databaseForm = new QFormLayout();
    databaseForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    databaseForm->setFormAlignment(Qt::AlignTop);
    databaseForm->setSpacing(10);

    m_useMySqlCheckBox = new QCheckBox(textFor("优先使用 MySQL 数据源", "Use MySQL data source"), databaseTab);
    m_hostEdit = new QLineEdit(databaseTab);
    m_portSpinBox = new QSpinBox(databaseTab);
    m_portSpinBox->setRange(1, 65535);
    m_databaseNameEdit = new QLineEdit(databaseTab);
    m_userNameEdit = new QLineEdit(databaseTab);
    m_passwordEdit = new QLineEdit(databaseTab);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_connectTimeoutSpinBox = new QSpinBox(databaseTab);
    m_connectTimeoutSpinBox->setRange(1, 60);
    m_connectTimeoutSpinBox->setSuffix(textFor(" 秒", " s"));
    m_readTimeoutSpinBox = new QSpinBox(databaseTab);
    m_readTimeoutSpinBox->setRange(1, 120);
    m_readTimeoutSpinBox->setSuffix(textFor(" 秒", " s"));
    m_defaultQueryDaysSpinBox = new QSpinBox(databaseTab);
    m_defaultQueryDaysSpinBox->setRange(1, 90);
    m_defaultQueryDaysSpinBox->setSuffix(textFor(" 天", " days"));
    m_autoRefreshCheckBox = new QCheckBox(textFor("启用自动刷新", "Enable auto refresh"), databaseTab);
    m_autoRefreshIntervalSpinBox = new QSpinBox(databaseTab);
    m_autoRefreshIntervalSpinBox->setRange(5, 3600);
    m_autoRefreshIntervalSpinBox->setSuffix(textFor(" 秒", " s"));
    m_pointDetailDirectoryRow = new QWidget(databaseTab);
    auto* pointDetailDirectoryLayout = new QHBoxLayout(m_pointDetailDirectoryRow);
    pointDetailDirectoryLayout->setContentsMargins(0, 0, 0, 0);
    pointDetailDirectoryLayout->setSpacing(8);
    m_pointDetailDirectoryEdit = new QLineEdit(m_pointDetailDirectoryRow);
    m_pointDetailDirectoryEdit->setPlaceholderText(textFor("留空时使用默认路径", "Leave empty to use the default path"));
    m_pointDetailBrowseButton = new QPushButton(textFor("选择文件夹", "Browse"), m_pointDetailDirectoryRow);
    UiTheme::applySecondaryButton(m_pointDetailBrowseButton);
    pointDetailDirectoryLayout->addWidget(m_pointDetailDirectoryEdit, 1);
    pointDetailDirectoryLayout->addWidget(m_pointDetailBrowseButton);
    m_pointDetailDefaultPathLabel = new QLabel(databaseTab);
    m_pointDetailDefaultPathLabel->setWordWrap(true);
    m_pointDetailDefaultPathLabel->setProperty("hint", QVariant(true));

    auto addDatabaseRow = [databaseForm](const QString& text, QWidget* field) {
        auto* label = new QLabel(text);
        label->setProperty("formLabel", QVariant(true));
        databaseForm->addRow(label, field);
    };

    databaseForm->addRow(QString(), m_useMySqlCheckBox);
    addDatabaseRow(textFor("主机", "Host"), m_hostEdit);
    addDatabaseRow(textFor("端口", "Port"), m_portSpinBox);
    addDatabaseRow(textFor("数据库名", "Database"), m_databaseNameEdit);
    addDatabaseRow(textFor("用户名", "User"), m_userNameEdit);
    addDatabaseRow(textFor("密码", "Password"), m_passwordEdit);
    addDatabaseRow(textFor("连接超时", "Connect timeout"), m_connectTimeoutSpinBox);
    addDatabaseRow(textFor("读取超时", "Read timeout"), m_readTimeoutSpinBox);
    addDatabaseRow(textFor("默认查询天数", "Default query range"), m_defaultQueryDaysSpinBox);
    databaseForm->addRow(QString(), m_autoRefreshCheckBox);
    addDatabaseRow(textFor("刷新周期", "Refresh interval"), m_autoRefreshIntervalSpinBox);
    addDatabaseRow(textFor("点位详情目录", "Point detail directory"), m_pointDetailDirectoryRow);
    addDatabaseRow(textFor("默认保存路径", "Default save path"), m_pointDetailDefaultPathLabel);
    databaseCardLayout->addLayout(databaseForm);
    databaseLayout->addWidget(databaseCard);
    databaseLayout->addStretch();

    auto* cleanupTab = new QWidget(this);
    auto* cleanupLayout = new QVBoxLayout(cleanupTab);
    cleanupLayout->setContentsMargins(14, 14, 14, 14);
    auto* cleanupHintLabel = new QLabel(
        textFor("数据清理仅支持 MySQL 数据源。可执行种子数据清理，也可按保留天数删除历史生产数据。",
                "Cleanup is available only for MySQL. Seed cleanup removes demo data. Production cleanup removes old records in batches."),
        cleanupTab);
    cleanupHintLabel->setProperty("hint", QVariant(true));
    cleanupHintLabel->setWordWrap(true);
    auto* cleanupCard = new QFrame(cleanupTab);
    UiTheme::applyPanel(cleanupCard);
    auto* cleanupCardLayout = new QVBoxLayout(cleanupCard);
    cleanupCardLayout->setContentsMargins(16, 16, 16, 16);
    cleanupCardLayout->setSpacing(12);
    auto* cleanupForm = new QFormLayout();
    cleanupForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cleanupForm->setFormAlignment(Qt::AlignTop);
    cleanupForm->setSpacing(10);

    m_cleanupRetentionDaysSpinBox = new QSpinBox(cleanupTab);
    m_cleanupRetentionDaysSpinBox->setRange(1, 3650);
    m_cleanupRetentionDaysSpinBox->setSuffix(textFor(" 天", " days"));
    auto* cleanupLabel = new QLabel(textFor("生产保留天数", "Production retention"));
    cleanupLabel->setProperty("formLabel", QVariant(true));
    cleanupForm->addRow(cleanupLabel, m_cleanupRetentionDaysSpinBox);

    auto* cleanupButtonLayout = new QHBoxLayout();
    cleanupButtonLayout->setSpacing(10);
    m_cleanupSeedButton = new QPushButton(textFor("清理种子数据", "Cleanup seed data"), cleanupTab);
    m_cleanupProductionButton = new QPushButton(textFor("清理生产数据", "Cleanup production data"), cleanupTab);
    UiTheme::applySecondaryButton(m_cleanupSeedButton);
    UiTheme::applyPrimaryButton(m_cleanupProductionButton);
    cleanupButtonLayout->addWidget(m_cleanupSeedButton);
    cleanupButtonLayout->addWidget(m_cleanupProductionButton);
    cleanupButtonLayout->addStretch();

    m_cleanupStatusLabel = new QLabel(textFor("执行前请确认数据库连接配置正确。",
                                              "Verify database settings before running cleanup."),
                                      cleanupTab);
    m_cleanupStatusLabel->setWordWrap(true);
    UiTheme::applyStatusLabel(m_cleanupStatusLabel, StatusTone::Neutral);

    cleanupCardLayout->addLayout(cleanupForm);
    cleanupCardLayout->addLayout(cleanupButtonLayout);
    cleanupCardLayout->addWidget(m_cleanupStatusLabel);
    cleanupLayout->addWidget(cleanupHintLabel);
    cleanupLayout->addWidget(cleanupCard);
    cleanupLayout->addStretch();

    auto* reportTab = new QWidget(this);
    auto* reportLayout = new QVBoxLayout(reportTab);
    reportLayout->setContentsMargins(14, 14, 14, 14);
    auto* reportHintLabel = new QLabel(
        textFor("这里的模板配置会写入报告包，用于客户名称、页脚说明和 Logo 路径。",
                "Template settings are written to the report package and control customer name, footer text, and logo path."),
        reportTab);
    reportHintLabel->setProperty("hint", QVariant(true));
    reportHintLabel->setWordWrap(true);
    auto* reportCard = new QFrame(reportTab);
    UiTheme::applyPanel(reportCard);
    auto* reportCardLayout = new QVBoxLayout(reportCard);
    reportCardLayout->setContentsMargins(16, 16, 16, 16);
    auto* reportForm = new QFormLayout();
    reportForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    reportForm->setFormAlignment(Qt::AlignTop);
    reportForm->setSpacing(10);

    m_reportTitleEdit = new QLineEdit(reportTab);
    m_customerNameEdit = new QLineEdit(reportTab);
    m_footerTextEdit = new QLineEdit(reportTab);
    m_logoPathEdit = new QLineEdit(reportTab);
    m_logoPathEdit->setPlaceholderText(textFor("例如：D:/assets/logo.png", "Example: D:/assets/logo.png"));

    auto addReportRow = [reportForm](const QString& text, QWidget* field) {
        auto* label = new QLabel(text);
        label->setProperty("formLabel", QVariant(true));
        reportForm->addRow(label, field);
    };

    addReportRow(textFor("报告标题", "Report title"), m_reportTitleEdit);
    addReportRow(textFor("客户名称", "Customer name"), m_customerNameEdit);
    addReportRow(textFor("页脚说明", "Footer text"), m_footerTextEdit);
    addReportRow(textFor("Logo 路径", "Logo path"), m_logoPathEdit);
    reportLayout->addWidget(reportHintLabel);
    reportCardLayout->addLayout(reportForm);
    reportLayout->addWidget(reportCard);
    reportLayout->addStretch();

    auto* otherTab = new QWidget(this);
    auto* otherLayout = new QVBoxLayout(otherTab);
    otherLayout->setContentsMargins(14, 14, 14, 14);
    auto* otherHintLabel = new QLabel(
        textFor("中英文与主题切换统一放在这里，修改后点击保存生效。默认主题为黑夜。",
                "Language and theme are managed here. Save to apply changes. The default theme is Night."),
        otherTab);
    otherHintLabel->setProperty("hint", QVariant(true));
    otherHintLabel->setWordWrap(true);
    auto* otherCard = new QFrame(otherTab);
    UiTheme::applyPanel(otherCard);
    auto* otherCardLayout = new QVBoxLayout(otherCard);
    otherCardLayout->setContentsMargins(16, 16, 16, 16);
    auto* otherForm = new QFormLayout();
    otherForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    otherForm->setFormAlignment(Qt::AlignTop);
    otherForm->setSpacing(10);

    m_languageCombo = new QComboBox(otherTab);
    m_languageCombo->addItem(textFor("中文", "Chinese"), static_cast<int>(LaserSpc::Infrastructure::AppLanguage::Chinese));
    m_languageCombo->addItem(textFor("英文", "English"), static_cast<int>(LaserSpc::Infrastructure::AppLanguage::English));
    m_themeCombo = new QComboBox(otherTab);
    m_themeCombo->addItem(textFor("黑夜", "Night"), static_cast<int>(LaserSpc::Infrastructure::ThemeStyle::Night));
    m_themeCombo->addItem(textFor("海蓝", "Ocean"), static_cast<int>(LaserSpc::Infrastructure::ThemeStyle::Ocean));
    m_themeCombo->addItem(textFor("石墨", "Graphite"), static_cast<int>(LaserSpc::Infrastructure::ThemeStyle::Graphite));
    m_themeCombo->addItem(textFor("暖砂", "Sand"), static_cast<int>(LaserSpc::Infrastructure::ThemeStyle::Sand));
    m_themeCombo->addItem(textFor("森林", "Forest"), static_cast<int>(LaserSpc::Infrastructure::ThemeStyle::Forest));
    m_themeCombo->addItem(textFor("余烬", "Ember"), static_cast<int>(LaserSpc::Infrastructure::ThemeStyle::Ember));
    m_themeCombo->addItem(textFor("极光", "Aurora"), static_cast<int>(LaserSpc::Infrastructure::ThemeStyle::Aurora));

    auto addOtherRow = [otherForm](const QString& text, QWidget* field) {
        auto* label = new QLabel(text);
        label->setProperty("formLabel", QVariant(true));
        otherForm->addRow(label, field);
    };

    addOtherRow(textFor("界面语言", "Language"), m_languageCombo);
    addOtherRow(textFor("主题风格", "Theme"), m_themeCombo);
    otherCardLayout->addLayout(otherForm);
    otherLayout->addWidget(otherHintLabel);
    otherLayout->addWidget(otherCard);
    otherLayout->addStretch();

    m_tabWidget->addTab(databaseTab, textFor("数据源", "Database"));
    m_tabWidget->addTab(cleanupTab, textFor("数据清理", "Cleanup"));
    m_tabWidget->addTab(reportTab, textFor("报告模板", "Report Template"));
    m_tabWidget->addTab(otherTab, textFor("其他设置", "Other Settings"));

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Save)->setText(textFor("保存", "Save"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(textFor("取消", "Cancel"));
    UiTheme::applyPrimaryButton(buttonBox->button(QDialogButtonBox::Save));
    UiTheme::applySecondaryButton(buttonBox->button(QDialogButtonBox::Cancel));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_useMySqlCheckBox, &QCheckBox::toggled, this, &SettingsDialog::updateFieldState);
    connect(m_autoRefreshCheckBox, &QCheckBox::toggled, this, &SettingsDialog::updateFieldState);
    connect(m_pointDetailDirectoryEdit, &QLineEdit::textChanged, this, &SettingsDialog::updatePointDetailPathHint);
    connect(m_pointDetailBrowseButton, &QPushButton::clicked, this, [this]() {
        const QString startPath = m_pointDetailDirectoryEdit->text().trimmed().isEmpty()
                                      ? QDir(QCoreApplication::applicationDirPath()).filePath("point_details")
                                      : m_pointDetailDirectoryEdit->text().trimmed();
        const QString selectedPath = QFileDialog::getExistingDirectory(
            this,
            textFor("选择点位详情目录", "Select Point Detail Directory"),
            startPath,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!selectedPath.trimmed().isEmpty()) {
            m_pointDetailDirectoryEdit->setText(QDir(selectedPath).absolutePath());
        }
    });
    connect(m_cleanupSeedButton, &QPushButton::clicked, this, &SettingsDialog::runSeedCleanup);
    connect(m_cleanupProductionButton, &QPushButton::clicked, this, &SettingsDialog::runProductionCleanup);
    connect(&m_cleanupWatcher,
            &QFutureWatcher<LaserSpc::Infrastructure::DataCleanupResult>::finished,
            this,
            &SettingsDialog::finishCleanupTask);

    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(infoLabel);
    rootLayout->addWidget(m_tabWidget, 1);
    rootLayout->addWidget(buttonBox);
}

void SettingsDialog::loadSettings(const LaserSpc::Infrastructure::AppSettings& settings) {
    m_useMySqlCheckBox->setChecked(settings.useMySql);
    m_hostEdit->setText(settings.database.host);
    m_portSpinBox->setValue(settings.database.port);
    m_databaseNameEdit->setText(settings.database.databaseName);
    m_userNameEdit->setText(settings.database.userName);
    m_passwordEdit->setText(settings.database.password);
    m_connectTimeoutSpinBox->setValue(settings.database.connectTimeoutSeconds);
    m_readTimeoutSpinBox->setValue(settings.database.readTimeoutSeconds);
    m_connectOptions = settings.database.connectOptions;
    m_defaultQueryDaysSpinBox->setValue(settings.defaultQueryDays);
    m_autoRefreshCheckBox->setChecked(settings.autoRefreshEnabled);
    m_autoRefreshIntervalSpinBox->setValue(settings.autoRefreshIntervalSeconds);
    m_pointDetailDirectoryEdit->setText(settings.pointDetailDirectory);
    updatePointDetailPathHint();
    m_cleanupRetentionDaysSpinBox->setValue(settings.cleanup.productionRetentionDays);
    m_reportTitleEdit->setText(settings.reportTemplate.reportTitle);
    m_customerNameEdit->setText(settings.reportTemplate.customerName);
    m_footerTextEdit->setText(settings.reportTemplate.footerText);
    m_logoPathEdit->setText(settings.reportTemplate.logoPath);
    if (m_languageCombo != nullptr) {
        m_languageCombo->setCurrentIndex(m_languageCombo->findData(static_cast<int>(settings.ui.language)));
    }
    if (m_themeCombo != nullptr) {
        m_themeCombo->setCurrentIndex(m_themeCombo->findData(static_cast<int>(settings.ui.themeStyle)));
    }
}

LaserSpc::Infrastructure::AppSettings SettingsDialog::settings() const {
    LaserSpc::Infrastructure::AppSettings value;
    value.useMySql = m_useMySqlCheckBox->isChecked();
    value.defaultQueryDays = m_defaultQueryDaysSpinBox->value();
    value.autoRefreshEnabled = m_autoRefreshCheckBox->isChecked();
    value.autoRefreshIntervalSeconds = m_autoRefreshIntervalSpinBox->value();
    value.pointDetailDirectory = m_pointDetailDirectoryEdit->text().trimmed();
    value.cleanup.productionRetentionDays = m_cleanupRetentionDaysSpinBox->value();
    value.database.host = m_hostEdit->text().trimmed();
    value.database.port = m_portSpinBox->value();
    value.database.databaseName = m_databaseNameEdit->text().trimmed();
    value.database.userName = m_userNameEdit->text().trimmed();
    value.database.password = m_passwordEdit->text();
    value.database.connectTimeoutSeconds = m_connectTimeoutSpinBox->value();
    value.database.readTimeoutSeconds = m_readTimeoutSpinBox->value();
    value.database.connectOptions = m_connectOptions;
    value.reportTemplate.reportTitle = m_reportTitleEdit->text().trimmed();
    value.reportTemplate.customerName = m_customerNameEdit->text().trimmed();
    value.reportTemplate.footerText = m_footerTextEdit->text().trimmed();
    value.reportTemplate.logoPath = m_logoPathEdit->text().trimmed();
    value.ui.language = static_cast<LaserSpc::Infrastructure::AppLanguage>(m_languageCombo->currentData().toInt());
    value.ui.themeStyle = static_cast<LaserSpc::Infrastructure::ThemeStyle>(m_themeCombo->currentData().toInt());
    return value;
}

void SettingsDialog::updatePointDetailPathHint() {
    const QString configuredPath = m_pointDetailDirectoryEdit->text().trimmed();
    const QString fallbackPath = QDir(QCoreApplication::applicationDirPath()).filePath("point_details");
    const QString effectivePath = configuredPath.isEmpty() ? fallbackPath : QDir(configuredPath).absolutePath();
    m_pointDetailDefaultPathLabel->setText(effectivePath);
}

void SettingsDialog::updateFieldState() {
    const bool mysqlEnabled = m_useMySqlCheckBox->isChecked();
    const bool cleanupIdle = !m_cleanupWatcher.isRunning();
    m_hostEdit->setEnabled(mysqlEnabled);
    m_portSpinBox->setEnabled(mysqlEnabled);
    m_databaseNameEdit->setEnabled(mysqlEnabled);
    m_userNameEdit->setEnabled(mysqlEnabled);
    m_passwordEdit->setEnabled(mysqlEnabled);
    m_connectTimeoutSpinBox->setEnabled(mysqlEnabled);
    m_readTimeoutSpinBox->setEnabled(mysqlEnabled);
    m_pointDetailDirectoryEdit->setEnabled(cleanupIdle);
    m_pointDetailBrowseButton->setEnabled(cleanupIdle);
    m_cleanupRetentionDaysSpinBox->setEnabled(mysqlEnabled && cleanupIdle);
    m_cleanupSeedButton->setEnabled(mysqlEnabled && cleanupIdle);
    m_cleanupProductionButton->setEnabled(mysqlEnabled && cleanupIdle);
    m_autoRefreshIntervalSpinBox->setEnabled(m_autoRefreshCheckBox->isChecked());
}

void SettingsDialog::runSeedCleanup() {
    const auto currentSettings = settings();
    if (!currentSettings.useMySql) {
        UiTheme::applyStatusLabel(m_cleanupStatusLabel, StatusTone::Warning);
        m_cleanupStatusLabel->setText(textFor("当前未启用 MySQL，无法执行数据清理。",
                                              "MySQL is not enabled, so cleanup is unavailable."));
        return;
    }

    const int answer = QMessageBox::warning(this,
                                            textFor("确认清理", "Confirm Cleanup"),
                                            textFor("种子清理会永久删除演示数据，是否继续？",
                                                    "Seed cleanup will permanently remove demo records. Continue?"),
                                            QMessageBox::Yes | QMessageBox::No,
                                            QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    startCleanupTask(CleanupMode::Seed, currentSettings.database);
}

void SettingsDialog::runProductionCleanup() {
    const auto currentSettings = settings();
    if (!currentSettings.useMySql) {
        UiTheme::applyStatusLabel(m_cleanupStatusLabel, StatusTone::Warning);
        m_cleanupStatusLabel->setText(textFor("当前未启用 MySQL，无法执行数据清理。",
                                              "MySQL is not enabled, so cleanup is unavailable."));
        return;
    }

    const int retentionDays = m_cleanupRetentionDaysSpinBox->value();
    const int answer = QMessageBox::warning(
        this,
        textFor("确认清理", "Confirm Cleanup"),
        textFor("生产数据清理会删除 %1 天之前的记录，是否继续？",
                "Production cleanup will remove records older than %1 days. Continue?").arg(retentionDays),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    startCleanupTask(CleanupMode::Production, currentSettings.database, retentionDays);
}

void SettingsDialog::startCleanupTask(CleanupMode mode,
                                      const LaserSpc::Infrastructure::DatabaseSettings& databaseSettings,
                                      int retentionDays) {
    if (m_cleanupWatcher.isRunning()) {
        return;
    }

    m_cleanupMode = mode;
    UiTheme::applyStatusLabel(m_cleanupStatusLabel, StatusTone::Neutral);
    if (mode == CleanupMode::Seed) {
        m_cleanupStatusLabel->setText(textFor("正在后台清理种子数据...", "Cleaning seed data in the background..."));
    } else {
        m_cleanupStatusLabel->setText(textFor("正在后台清理生产数据...", "Cleaning production data in the background..."));
    }
    updateFieldState();

    auto future = QtConcurrent::run([mode, databaseSettings, retentionDays]() {
        if (mode == CleanupMode::Seed) {
            return LaserSpc::Infrastructure::DataCleanupService::cleanupSeedData(databaseSettings);
        }
        return LaserSpc::Infrastructure::DataCleanupService::cleanupProductionDataOlderThan(databaseSettings, retentionDays);
    });
    m_cleanupWatcher.setFuture(future);
}

void SettingsDialog::finishCleanupTask() {
    const auto result = m_cleanupWatcher.result();
    const CleanupMode mode = m_cleanupMode;
    m_cleanupMode = CleanupMode::None;

    if (!result.success) {
        if (mode == CleanupMode::Seed) {
            LaserSpc::Infrastructure::Logger::error("Seed cleanup failed: " + result.errorMessage);
        } else {
            LaserSpc::Infrastructure::Logger::error("Production cleanup failed: " + result.errorMessage);
        }

        UiTheme::applyStatusLabel(m_cleanupStatusLabel, StatusTone::Danger);
        m_cleanupStatusLabel->setText(textFor("数据清理失败：%1", "Cleanup failed: %1").arg(result.errorMessage));
        updateFieldState();
        return;
    }

    QString message;
    if (mode == CleanupMode::Seed) {
        message = textFor("种子清理完成：删除 %1 条点位记录，删除 %2 条单板记录。",
                          "Seed cleanup completed: deleted %1 point rows and %2 board rows.")
                      .arg(result.deletedPointRecords)
                      .arg(result.deletedBoardRecords);
    } else {
        message = textFor("生产清理完成：截止 %1，删除 %2 条点位记录，删除 %3 条单板记录。",
                          "Production cleanup completed before %1: deleted %2 point rows and %3 board rows.")
                      .arg(result.cutoffTime.toString("yyyy-MM-dd HH:mm:ss"))
                      .arg(result.deletedPointRecords)
                      .arg(result.deletedBoardRecords);
    }
    if (result.retryCount > 0) {
        message += textFor(" 已自动重试 %1 次。", " Auto-retried %1 times.").arg(result.retryCount);
    }

    UiTheme::applyStatusLabel(m_cleanupStatusLabel, StatusTone::Success);
    m_cleanupStatusLabel->setText(message);
    emit dataCleanupFinished(message);
    updateFieldState();
}

}  // namespace LaserSpc::Ui
