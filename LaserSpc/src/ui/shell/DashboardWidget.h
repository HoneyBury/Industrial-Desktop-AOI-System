#pragma once

#include <QDateTime>
#include <QFutureWatcher>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>

#include "app/AppServiceFacade.h"
#include "infrastructure/AppConfigService.h"
#include "ui/pages/PageAsyncSupport.h"
#include "ui/shell/DashboardPageController.h"

class QAction;

namespace LaserSpc::Ui {

class BasePage;
class BadStatPage;
class FilterPanel;
class MainShellMediator;
class PointRecordPage;
class SummaryPage;
class BoardRecordPage;
class DashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit DashboardWidget(LaserSpc::App::AppServiceFacade* facade, QWidget* parent = nullptr);
    void refreshData();
    void refreshFilterOptions();

private:
    void setupUi();
    void setupConnections();
    void handleFilterOptionsFinished();
    void reloadCurrentPage(const LaserSpc::Domain::FilterCriteria& criteria);
    void navigateToPage(LaserSpc::Domain::PageId pageId,
                        const LaserSpc::Domain::FilterCriteria& criteria,
                        const QString& statusMessage);
    BasePage* currentPage() const;
    void applyAutoRefreshSettings();
    void updateFilterOptionsUiState(PageLoadState state, const QString& detail = QString());
    void updateStatus(const QString& message);
    void updateWorkspaceSummary();
    void refreshRuntimeStatus();
    void refreshShellTexts();
    void applyLanguage();
    void applyTheme();
    void prepareDialog(QWidget* dialog) const;
    QString pageTitleFor(LaserSpc::Domain::PageId pageId) const;
    bool isEnglish() const;
    void openSettingsDialog();
    void openDiagnosticsDialog();
    void openMesConfigDialog();

    LaserSpc::App::AppServiceFacade* m_facade = nullptr;
    LaserSpc::Infrastructure::AppConfigService m_configService;
    DashboardPageController* m_pageController = nullptr;
    MainShellMediator* m_mediator = nullptr;
    QListWidget* m_navigation = nullptr;
    QStackedWidget* m_stack = nullptr;
    BadStatPage* m_badStatPage = nullptr;
    BoardRecordPage* m_boardRecordPage = nullptr;
    FilterPanel* m_filterPanel = nullptr;
    PointRecordPage* m_pointRecordPage = nullptr;
    SummaryPage* m_summaryPage = nullptr;
    QLabel* m_navigationTitleLabel = nullptr;
    QLabel* m_sidebarOverviewLabel = nullptr;
    QLabel* m_sidebarSourceTitleLabel = nullptr;
    QLabel* m_sidebarSourceValueLabel = nullptr;
    QLabel* m_sidebarRangeTitleLabel = nullptr;
    QLabel* m_sidebarRangeValueLabel = nullptr;
    QLabel* m_sidebarRefreshTitleLabel = nullptr;
    QLabel* m_sidebarRefreshValueLabel = nullptr;
    QLabel* m_sidebarRuntimeTitleLabel = nullptr;
    QLabel* m_sidebarRuntimeValueLabel = nullptr;
    QLabel* m_sidebarFocusTitleLabel = nullptr;
    QLabel* m_sidebarFocusValueLabel = nullptr;
    QLabel* m_sidebarActionTitleLabel = nullptr;
    QLabel* m_sidebarActionValueLabel = nullptr;
    QLabel* m_sidebarPageTitleLabel = nullptr;
    QLabel* m_sidebarPageValueLabel = nullptr;
    QLabel* m_sidebarPresetTitleLabel = nullptr;
    QLabel* m_sidebarPresetValueLabel = nullptr;
    QLabel* m_pageTitleLabel = nullptr;
    QLabel* m_pageMetaLabel = nullptr;
    QPushButton* m_configButton = nullptr;
    ::QAction* m_mesConfigAction = nullptr;
    ::QAction* m_settingsAction = nullptr;
    ::QAction* m_diagnosticsAction = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_dataSourceLabel = nullptr;
    QLabel* m_autoRefreshLabel = nullptr;
    QLabel* m_lastRefreshLabel = nullptr;
    QLabel* m_runtimeStatusLabel = nullptr;
    QTimer* m_autoRefreshTimer = nullptr;
    QFutureWatcher<FilterOptionsTaskResult>* m_filterOptionsWatcher = nullptr;
    PageLoadState m_filterOptionsLoadState = PageLoadState::Idle;
    QDateTime m_lastRefreshTime;
    LaserSpc::Infrastructure::AppSettings m_uiSettings;
    int m_runtimeWarningCount = 0;
};

}  // namespace LaserSpc::Ui
