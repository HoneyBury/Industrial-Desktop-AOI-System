#include "ui/shell/DashboardWidget.h"
#include <QVariant>
#include <array>
#include <QAction>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>
#include "infrastructure/ExportService.h"
#include "infrastructure/Logger.h"
#include "infrastructure/RepositoryFactory.h"
#include "infrastructure/RuntimeDiagnostics.h"
#include "ui/common/StatusText.h"
#include "ui/common/UiTheme.h"
#include "ui/dialogs/DiagnosticsDialog.h"
#include "ui/dialogs/MesConfigDialog.h"
#include "ui/dialogs/SettingsDialog.h"
#include "ui/pages/BadStatPage.h"
#include "ui/pages/BasePage.h"
#include "ui/pages/BoardRecordPage.h"
#include "ui/pages/PageAsyncSupport.h"
#include "ui/pages/PointRecordPage.h"
#include "ui/pages/SummaryPage.h"
#include "ui/shell/MainShellMediator.h"
#include "ui/widgets/FilterPanel.h"

namespace LaserSpc::Ui {
namespace {
class StableStackedWidget : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;

    QSize sizeHint() const override {
        return stackSizeHint(&QWidget::sizeHint, QStackedWidget::sizeHint());
    }

    QSize minimumSizeHint() const override {
        return stackSizeHint(&QWidget::minimumSizeHint, QStackedWidget::minimumSizeHint());
    }

private:
    using SizeHintMember = QSize (QWidget::*)() const;

    QSize stackSizeHint(SizeHintMember member, const QSize& fallback) const {
        QSize hint = fallback;
        if (QWidget* widget = currentWidget()) {
            hint = (widget->*member)();
        }
        hint.setWidth(qMax(0, hint.width()));
        hint.setHeight(qMax(0, hint.height()));
        return hint;
    }
};

QString pick(bool en, const QString& zh, const QString& enText) { return en ? enText : zh; }
QString navDesc(LaserSpc::Domain::PageId id, bool en) {
    switch (id) {
        case LaserSpc::Domain::PageId::BadStat: return pick(en, QObject::tr("不良点与读码等级"), QObject::tr("Bad points and read grades"));
        case LaserSpc::Domain::PageId::BoardRecord: return pick(en, QObject::tr("单板级追溯记录"), QObject::tr("Board traceability records"));
        case LaserSpc::Domain::PageId::PointRecord: return pick(en, QObject::tr("点位明细与节拍"), QObject::tr("Point detail and timing"));
        case LaserSpc::Domain::PageId::Summary:
        default: return pick(en, QObject::tr("良率、产出与趋势"), QObject::tr("Yield, output and trends"));
    }
}
LaserSpc::Domain::PageId pageIdForRow(int row) {
    switch (row) {
        case 1: return LaserSpc::Domain::PageId::BadStat;
        case 2: return LaserSpc::Domain::PageId::BoardRecord;
        case 3: return LaserSpc::Domain::PageId::PointRecord;
        default: return LaserSpc::Domain::PageId::Summary;
    }
}
QIcon pageIcon(QWidget* w, LaserSpc::Domain::PageId id) {
    if (w == nullptr || w->style() == nullptr) return {};
    switch (id) {
        case LaserSpc::Domain::PageId::BadStat: return w->style()->standardIcon(QStyle::SP_MessageBoxWarning);
        case LaserSpc::Domain::PageId::BoardRecord: return w->style()->standardIcon(QStyle::SP_FileDialogContentsView);
        case LaserSpc::Domain::PageId::PointRecord: return w->style()->standardIcon(QStyle::SP_DialogYesButton);
        default: return w->style()->standardIcon(QStyle::SP_ComputerIcon);
    }
}
QLabel* metricValue(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setProperty("hint", QVariant(true));
    label->setWordWrap(true);
    return label;
}
void addMetric(QGridLayout* layout, QWidget* parent, const QString& title, QLabel** titleOut, QLabel** valueOut, int row, int col) {
    auto* titleLabel = new QLabel(title, parent);
    titleLabel->setProperty("metricTitle", QVariant(true));
    auto* value = metricValue(parent);
    layout->addWidget(titleLabel, row, col);
    layout->addWidget(value, row + 1, col);
    if (titleOut != nullptr) {
        *titleOut = titleLabel;
    }
    *valueOut = value;
}

QList<PageInsightMetric> fallbackInsights(LaserSpc::Domain::PageId pageId, bool en) {
    switch (pageId) {
        case LaserSpc::Domain::PageId::BadStat:
            return {
                {pick(en, QObject::tr("统计"), QObject::tr("Stats")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("热点"), QObject::tr("Hot spot")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("等级"), QObject::tr("Grades")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("操作"), QObject::tr("Action")), pick(en, QObject::tr("双击表格钻取"), QObject::tr("Double-click table rows"))}
            };
        case LaserSpc::Domain::PageId::BoardRecord:
            return {
                {pick(en, QObject::tr("记录"), QObject::tr("Records")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("结果"), QObject::tr("Result")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("线体"), QObject::tr("Line")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("操作"), QObject::tr("Action")), pick(en, QObject::tr("双击记录钻取"), QObject::tr("Double-click rows"))}
            };
        case LaserSpc::Domain::PageId::PointRecord:
            return {
                {pick(en, QObject::tr("记录"), QObject::tr("Records")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("等级"), QObject::tr("Grade")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("设备"), QObject::tr("Device")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("操作"), QObject::tr("Action")), pick(en, QObject::tr("图表点击定位"), QObject::tr("Click charts to locate"))}
            };
        case LaserSpc::Domain::PageId::Summary:
        default:
            return {
                {pick(en, QObject::tr("产出"), QObject::tr("Output")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("良率"), QObject::tr("Yield")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("热点"), QObject::tr("Hot spot")), pick(en, QObject::tr("等待加载"), QObject::tr("Waiting"))},
                {pick(en, QObject::tr("操作"), QObject::tr("Action")), pick(en, QObject::tr("双击表格钻取"), QObject::tr("Double-click rows"))}
            };
    }
}
bool sameDateTime(const QDateTime& a, const QDateTime& b) {
    return a.isValid() == b.isValid() && (!a.isValid() || a.toSecsSinceEpoch() == b.toSecsSinceEpoch());
}
bool matchesDefault(const LaserSpc::Domain::FilterCriteria& c, const LaserSpc::Domain::FilterCriteria& d) {
    return sameDateTime(c.beginTime, d.beginTime) && sameDateTime(c.endTime, d.endTime) && c.lineName == d.lineName &&
           c.programName == d.programName && c.deviceName == d.deviceName && c.result == d.result &&
           c.keyword.trimmed() == d.keyword.trimmed();
}
QString criteriaDigest(const LaserSpc::Domain::FilterCriteria& c, bool en) {
    const QString all = QObject::tr("全部");
    const QString line = c.lineName == all ? pick(en, QObject::tr("全部线体"), QObject::tr("All lines")) : c.lineName;
    const QString program = c.programName == all ? pick(en, QObject::tr("全部程序"), QObject::tr("All programs")) : c.programName;
    return QObject::tr("%1 | %2").arg(line, program);
}

QString compactRuntimeText(bool en, int warningCount) {
    return warningCount == 0 ? pick(en, QObject::tr("正常"), QObject::tr("Healthy"))
                             : pick(en, QObject::tr("%1 条告警").arg(warningCount), QObject::tr("%1 warning(s)").arg(warningCount));
}
}

DashboardWidget::DashboardWidget(LaserSpc::App::AppServiceFacade* facade, QWidget* parent)
    : QWidget(parent), m_facade(facade), m_uiSettings(m_configService.settings()) {
    applyLanguage();
    setupUi();
    setupConnections();
    applyTheme();
    refreshShellTexts();
    refreshFilterOptions();
    applyAutoRefreshSettings();
    m_navigation->setCurrentRow(0);
    reloadCurrentPage(m_filterPanel->criteria());
}

void DashboardWidget::refreshData() {
    m_pageController->refreshData();
}

void DashboardWidget::setupUi() {
    setObjectName(QObject::tr("LaserSpcDashboard"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);
    auto* body = new QHBoxLayout();
    body->setSpacing(12);
    auto* navFrame = new QFrame(this);
    UiTheme::applyPanel(navFrame);
    navFrame->setMinimumWidth(272);
    navFrame->setMaximumWidth(380);
    auto* nav = new QVBoxLayout(navFrame);
    nav->setContentsMargins(12, 12, 12, 12);
    nav->setSpacing(12);

    auto* overview = new QFrame(navFrame);
    UiTheme::applyPanel(overview);
    overview->setProperty("metricCard", QVariant(true));
    overview->setProperty("navOverview", QVariant(true));
    auto* overviewLayout = new QVBoxLayout(overview);
    overviewLayout->setContentsMargins(12, 12, 12, 12);
    m_sidebarOverviewLabel = new QLabel(overview);
    m_sidebarOverviewLabel->setProperty("metricTitle", QVariant(true));
    auto* grid = new QGridLayout();
    addMetric(grid, overview, QObject::tr("Source"), &m_sidebarSourceTitleLabel, &m_sidebarSourceValueLabel, 0, 0);
    addMetric(grid, overview, QObject::tr("Range"), &m_sidebarRangeTitleLabel, &m_sidebarRangeValueLabel, 0, 1);
    addMetric(grid, overview, QObject::tr("Refresh"), &m_sidebarRefreshTitleLabel, &m_sidebarRefreshValueLabel, 2, 0);
    addMetric(grid, overview, QObject::tr("Runtime"), &m_sidebarRuntimeTitleLabel, &m_sidebarRuntimeValueLabel, 2, 1);
    overviewLayout->addWidget(m_sidebarOverviewLabel);
    overviewLayout->addLayout(grid);

    m_navigationTitleLabel = new QLabel(navFrame);
    m_navigationTitleLabel->setProperty("hint", QVariant(true));
    m_navigation = new QListWidget(navFrame);
    m_navigation->addItems({QString(), QString(), QString(), QString()});
    m_navigation->setIconSize(QSize(18, 18));
    m_navigation->setSpacing(6);
    m_navigation->setWordWrap(false);
    m_navigation->setTextElideMode(Qt::ElideNone);
    m_navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* insight = new QFrame(navFrame);
    UiTheme::applyPanel(insight);
    insight->setProperty("metricCard", QVariant(true));
    auto* insightGrid = new QGridLayout(insight);
    insightGrid->setContentsMargins(12, 12, 12, 12);
    addMetric(insightGrid, insight, QObject::tr("Focus"), &m_sidebarFocusTitleLabel, &m_sidebarFocusValueLabel, 0, 0);
    addMetric(insightGrid, insight, QObject::tr("Page"), &m_sidebarPageTitleLabel, &m_sidebarPageValueLabel, 0, 1);
    addMetric(insightGrid, insight, QObject::tr("Action"), &m_sidebarActionTitleLabel, &m_sidebarActionValueLabel, 2, 0);
    addMetric(insightGrid, insight, QObject::tr("Preset"), &m_sidebarPresetTitleLabel, &m_sidebarPresetValueLabel, 2, 1);

    nav->addWidget(overview);
    nav->addWidget(m_navigationTitleLabel);
    nav->addWidget(m_navigation, 1);
    nav->addWidget(insight);

    auto* content = new QFrame(this);
    UiTheme::applyPanel(content);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(14, 14, 14, 14);
    contentLayout->setSpacing(12);
    auto* header = new QHBoxLayout();
    m_pageTitleLabel = new QLabel(content);
    m_pageTitleLabel->setProperty("sectionTitle", QVariant(true));
    m_pageMetaLabel = new QLabel(content);
    m_pageMetaLabel->setProperty("hint", QVariant(true));
    m_pageMetaLabel->setWordWrap(true);
    m_configButton = new QPushButton(content);
    UiTheme::applySecondaryButton(m_configButton);
    m_configButton->setProperty("configLauncher", QVariant(true));
    m_configButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_configButton->setMinimumWidth(122);
    m_configButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto* menu = new QMenu(m_configButton);
    m_settingsAction = menu->addAction(style()->standardIcon(QStyle::SP_FileDialogInfoView), QString());
    m_mesConfigAction = menu->addAction(style()->standardIcon(QStyle::SP_DriveNetIcon), QString());
    menu->addSeparator();
    m_diagnosticsAction = menu->addAction(style()->standardIcon(QStyle::SP_MessageBoxInformation), QString());
    m_configButton->setMenu(menu);
    UiTheme::applyPopupMenu(menu);
    header->addWidget(m_pageTitleLabel);
    header->addWidget(m_pageMetaLabel, 1);
    header->addWidget(m_configButton);

    m_filterPanel = new FilterPanel(m_facade->defaultFilter(), content);
    m_stack = new StableStackedWidget(content);
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_summaryPage = new SummaryPage(m_facade, m_stack);
    m_badStatPage = new BadStatPage(m_facade, m_stack);
    m_boardRecordPage = new BoardRecordPage(m_facade, m_stack);
    m_pointRecordPage = new PointRecordPage(m_facade, m_stack);
    for (BasePage* page : {static_cast<BasePage*>(m_summaryPage),
                           static_cast<BasePage*>(m_badStatPage),
                           static_cast<BasePage*>(m_boardRecordPage),
                           static_cast<BasePage*>(m_pointRecordPage)}) {
        page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    m_stack->addWidget(m_summaryPage);
    m_stack->addWidget(m_badStatPage);
    m_stack->addWidget(m_boardRecordPage);
    m_stack->addWidget(m_pointRecordPage);
    contentLayout->addLayout(header);
    contentLayout->addWidget(m_filterPanel);
    contentLayout->addWidget(m_stack, 1);
    body->addWidget(navFrame);
    body->addWidget(content, 1);

    auto* footer = new QFrame(this);
    UiTheme::applyPanel(footer);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(12, 8, 12, 8);
    footerLayout->setSpacing(12);
    m_statusLabel = new QLabel(QObject::tr("等待查询"), footer);
    m_dataSourceLabel = new QLabel(footer);
    m_autoRefreshLabel = new QLabel(footer);
    m_lastRefreshLabel = new QLabel(footer);
    m_runtimeStatusLabel = new QLabel(footer);
    for (QLabel* label : {m_statusLabel, m_dataSourceLabel, m_autoRefreshLabel, m_lastRefreshLabel, m_runtimeStatusLabel}) {
        label->setProperty("hint", QVariant(true));
    }
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_dataSourceLabel->setVisible(false);
    footerLayout->addWidget(m_statusLabel, 1);
    footerLayout->addWidget(m_autoRefreshLabel);
    footerLayout->addWidget(m_lastRefreshLabel);
    footerLayout->addWidget(m_runtimeStatusLabel);

    root->addLayout(body, 1);
    root->addWidget(footer);

    m_mediator = new MainShellMediator(this);
    m_autoRefreshTimer = new QTimer(this);
    m_filterOptionsWatcher = new QFutureWatcher<FilterOptionsTaskResult>(this);
    m_pageController = new DashboardPageController(m_facade,
                                                   m_filterPanel,
                                                   m_stack,
                                                   m_summaryPage,
                                                   m_badStatPage,
                                                   m_boardRecordPage,
                                                   m_pointRecordPage,
                                                   m_autoRefreshTimer,
                                                   m_filterOptionsWatcher,
                                                   this);
    updateFilterOptionsUiState(PageLoadState::Idle);
    refreshRuntimeStatus();
    updateWorkspaceSummary();
    if (m_settingsAction != nullptr) {
        m_settingsAction->setEnabled(m_facade->settings().systemSettingsEnabled);
    }
}

void DashboardWidget::setupConnections() {
    connect(m_navigation, &QListWidget::currentRowChanged, m_mediator, &MainShellMediator::onNavigationChanged);
    connect(m_mediator, &MainShellMediator::pageSwitchRequested, this, [this](int, LaserSpc::Domain::PageId pageId) {
        navigateToPage(pageId, m_filterPanel->criteria(), {});
    });
    connect(m_filterPanel, &FilterPanel::queryRequested, this, [this](const LaserSpc::Domain::FilterCriteria& c) { reloadCurrentPage(c); });
    connect(m_filterPanel, &FilterPanel::resetRequested, this, [this](const LaserSpc::Domain::FilterCriteria& c) { reloadCurrentPage(c); });
    connect(m_summaryPage, &SummaryPage::statusMessageChanged, this, [this](const QString& m) { updateStatus(m); });
    connect(m_badStatPage, &BadStatPage::statusMessageChanged, this, [this](const QString& m) { updateStatus(m); });
    connect(m_boardRecordPage, &BoardRecordPage::statusMessageChanged, this, [this](const QString& m) { updateStatus(m); });
    connect(m_pointRecordPage, &PointRecordPage::statusMessageChanged, this, [this](const QString& m) { updateStatus(m); });
    connect(m_summaryPage, &SummaryPage::boardDrillDownRequested, this, [this](const LaserSpc::Domain::FilterCriteria& c, const QString& m) { navigateToPage(LaserSpc::Domain::PageId::BoardRecord, c, m); });
    connect(m_badStatPage, &BadStatPage::pointDrillDownRequested, this, [this](const LaserSpc::Domain::FilterCriteria& c, const QString& m) { navigateToPage(LaserSpc::Domain::PageId::PointRecord, c, m); });
    connect(m_boardRecordPage, &BoardRecordPage::pointDrillDownRequested, this, [this](const LaserSpc::Domain::FilterCriteria& c, const QString& m) { navigateToPage(LaserSpc::Domain::PageId::PointRecord, c, m); });
    connect(m_settingsAction, &QAction::triggered, this, &DashboardWidget::openSettingsDialog);
    connect(m_diagnosticsAction, &QAction::triggered, this, &DashboardWidget::openDiagnosticsDialog);
    connect(m_mesConfigAction, &QAction::triggered, this, &DashboardWidget::openMesConfigDialog);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, [this]() {
        auto criteria = m_filterPanel->criteria();
        criteria.endTime = QDateTime::currentDateTime();
        m_filterPanel->setCriteria(criteria);
        reloadCurrentPage(criteria);
    });
    connect(m_filterOptionsWatcher, &QFutureWatcher<FilterOptionsTaskResult>::finished, this, &DashboardWidget::handleFilterOptionsFinished);
    connect(m_pageController, &DashboardPageController::statusMessageChanged, this, &DashboardWidget::updateStatus);
    connect(m_pageController, &DashboardPageController::pageChanged, this, [this](LaserSpc::Domain::PageId pageId) {
        m_pageTitleLabel->setText(pageTitleFor(pageId));
        updateWorkspaceSummary();
    });
    connect(m_pageController, &DashboardPageController::pageReloaded, this, [this]() {
        m_lastRefreshTime = QDateTime::currentDateTime();
    });
    connect(m_pageController, &DashboardPageController::filterOptionsStateChanged, this, &DashboardWidget::updateFilterOptionsUiState);
    connect(m_pageController, &DashboardPageController::filterOptionsApplied, this,
            [this](const LaserSpc::Domain::FilterOptions& options,
                   const LaserSpc::Domain::FilterCriteria& criteriaSnapshot,
                   bool enabled) {
                if (enabled || !options.lineNames.isEmpty() || !options.programNames.isEmpty() || !options.deviceNames.isEmpty()) {
                    m_filterPanel->setFilterOptions(options);
                }
                m_filterPanel->setCriteria(criteriaSnapshot);
                m_filterPanel->setEnabled(enabled);
            });
    connect(m_pageController, &DashboardPageController::layoutRefreshRequested, this, [this]() {
        m_stack->updateGeometry();
        updateGeometry();
    });
    connect(m_pageController, &DashboardPageController::workspaceSummaryRefreshRequested, this, &DashboardWidget::updateWorkspaceSummary);
}

void DashboardWidget::refreshFilterOptions() {
    m_pageController->refreshFilterOptions();
}

void DashboardWidget::handleFilterOptionsFinished() {
    m_pageController->handleFilterOptionsFinished();
}

void DashboardWidget::reloadCurrentPage(const LaserSpc::Domain::FilterCriteria& criteria) {
    m_pageController->reloadCurrentPage(criteria);
}

void DashboardWidget::navigateToPage(LaserSpc::Domain::PageId pageId, const LaserSpc::Domain::FilterCriteria& criteria, const QString& statusMessage) {
    const int target = m_pageController->indexForPageId(pageId);
    if (m_navigation->currentRow() != target) {
        m_navigation->blockSignals(true);
        m_navigation->setCurrentRow(target);
        m_navigation->blockSignals(false);
    }
    m_pageController->navigateToPage(pageId, criteria, statusMessage);
}

BasePage* DashboardWidget::currentPage() const { return static_cast<BasePage*>(m_stack->currentWidget()); }

void DashboardWidget::applyAutoRefreshSettings() {
    m_pageController->applyAutoRefreshSettings();
}

void DashboardWidget::updateFilterOptionsUiState(PageLoadState state, const QString& detail) {
    m_filterOptionsLoadState = state;
    Q_UNUSED(state);
    Q_UNUSED(detail);
}

void DashboardWidget::updateStatus(const QString& message) {
    LaserSpc::Infrastructure::Logger::info("UI status updated: " + message);
    m_statusLabel->setText(message);
    updateWorkspaceSummary();
}

void DashboardWidget::refreshRuntimeStatus() {
    const auto snapshot = LaserSpc::Infrastructure::RuntimeDiagnostics::collectSnapshot(
        m_configService.configFilePath(),
        LaserSpc::Infrastructure::ExportService::defaultExportDirectory());
    m_runtimeWarningCount = snapshot.warnings.size();
}

void DashboardWidget::updateWorkspaceSummary() {
    const auto settings = m_facade->settings();
    const auto current = m_filterPanel != nullptr ? m_filterPanel->criteria() : m_facade->defaultFilter();
    m_pageMetaLabel->setText(isEnglish()
                                 ? QObject::tr("Focus: %1 | Default window: last %2 day(s)").arg(criteriaDigest(current, true)).arg(settings.defaultQueryDays)
                                 : QObject::tr("当前筛选：%1 | 默认窗口：最近 %2 天").arg(criteriaDigest(current, false)).arg(settings.defaultQueryDays));
    m_dataSourceLabel->setText(isEnglish() ? QObject::tr("Source: %1").arg(m_facade->dataSourceMode()) : QObject::tr("数据源：%1").arg(m_facade->dataSourceMode()));
    m_autoRefreshLabel->setText(settings.autoRefreshEnabled ? pick(isEnglish(), QObject::tr("自动刷新：%1 秒").arg(settings.autoRefreshIntervalSeconds), QObject::tr("Auto refresh: %1 s").arg(settings.autoRefreshIntervalSeconds)) : pick(isEnglish(), QObject::tr("自动刷新：关闭"), QObject::tr("Auto refresh: off")));
    m_lastRefreshLabel->setText(m_lastRefreshTime.isValid() ? pick(isEnglish(), QObject::tr("最近刷新：%1").arg(m_lastRefreshTime.toString("HH:mm:ss")), QObject::tr("Last refresh: %1").arg(m_lastRefreshTime.toString("HH:mm:ss"))) : pick(isEnglish(), QObject::tr("最近刷新：未执行"), QObject::tr("Last refresh: not executed")));
    m_sidebarSourceValueLabel->setText(m_facade->dataSourceMode());
    m_sidebarRangeValueLabel->setText(pick(isEnglish(), QObject::tr("最近 %1 天").arg(settings.defaultQueryDays), QObject::tr("Last %1 day(s)").arg(settings.defaultQueryDays)));
    m_sidebarRefreshValueLabel->setText(m_lastRefreshTime.isValid() ? m_lastRefreshTime.toString("HH:mm:ss") : pick(isEnglish(), QObject::tr("待刷新"), QObject::tr("Pending")));
    m_runtimeStatusLabel->setText(pick(isEnglish(), QObject::tr("运行：%1").arg(compactRuntimeText(false, m_runtimeWarningCount)), QObject::tr("Runtime: %1").arg(compactRuntimeText(true, m_runtimeWarningCount))));
    m_sidebarRuntimeValueLabel->setText(compactRuntimeText(isEnglish(), m_runtimeWarningCount));
    if (m_settingsAction != nullptr) {
        m_settingsAction->setEnabled(settings.systemSettingsEnabled);
    }
    const auto currentPageId = pageIdForRow(m_navigation->currentRow());
    QList<PageInsightMetric> insights = currentPage() != nullptr ? currentPage()->pageInsights(isEnglish()) : QList<PageInsightMetric>{};
    if (insights.size() < 4) {
        insights = fallbackInsights(currentPageId, isEnglish());
    }
    const std::array<QLabel*, 4> titleLabels{m_sidebarFocusTitleLabel, m_sidebarPageTitleLabel, m_sidebarActionTitleLabel, m_sidebarPresetTitleLabel};
    const std::array<QLabel*, 4> valueLabels{m_sidebarFocusValueLabel, m_sidebarPageValueLabel, m_sidebarActionValueLabel, m_sidebarPresetValueLabel};
    for (int index = 0; index < 4; ++index) {
        QString title = insights.at(index).title;
        QString value = insights.at(index).value;
        if (currentPageId == LaserSpc::Domain::PageId::Summary && index == 0) {
            if (const int slashIndex = value.indexOf('/'); slashIndex >= 0) {
                value = value.left(slashIndex).trimmed();
            }
            if (!isEnglish()) {
                title = QObject::tr("总产出");
            }
        }
        if (currentPageId == LaserSpc::Domain::PageId::BadStat && index == 0 && !isEnglish()) {
            title = QObject::tr("不良总点数");
        }
        titleLabels.at(index)->setText(title);
        valueLabels.at(index)->setText(value);
    }
}

void DashboardWidget::refreshShellTexts() {
    m_sidebarOverviewLabel->setText(pick(isEnglish(), QObject::tr("工作区概览"), QObject::tr("Workspace Snapshot")));
    m_sidebarSourceTitleLabel->setText(pick(isEnglish(), QObject::tr("数据源"), QObject::tr("Source")));
    m_sidebarRangeTitleLabel->setText(pick(isEnglish(), QObject::tr("范围"), QObject::tr("Range")));
    m_sidebarRefreshTitleLabel->setText(pick(isEnglish(), QObject::tr("刷新"), QObject::tr("Refresh")));
    m_sidebarRuntimeTitleLabel->setText(pick(isEnglish(), QObject::tr("运行态"), QObject::tr("Runtime")));
    m_navigationTitleLabel->setText(pick(isEnglish(), QObject::tr("模块导航"), QObject::tr("Modules")));
    if (m_navigation->count() >= 4) {
        const std::array<LaserSpc::Domain::PageId, 4> ids{LaserSpc::Domain::PageId::Summary, LaserSpc::Domain::PageId::BadStat, LaserSpc::Domain::PageId::BoardRecord, LaserSpc::Domain::PageId::PointRecord};
        for (int i = 0; i < 4; ++i) {
            m_navigation->item(i)->setText(pageTitleFor(ids.at(i)) + QObject::tr("\n") + navDesc(ids.at(i), isEnglish()));
            m_navigation->item(i)->setIcon(pageIcon(this, ids.at(i)));
            m_navigation->item(i)->setSizeHint(QSize(0, 76));
        }
    }
    m_configButton->setText(pick(isEnglish(), QObject::tr("配置中心"), QObject::tr("Config Center")));
    m_settingsAction->setText(pick(isEnglish(), QObject::tr("系统设置"), QObject::tr("Settings")));
    m_mesConfigAction->setText(pick(isEnglish(), QObject::tr("MES 配置"), QObject::tr("MES Configuration")));
    m_diagnosticsAction->setText(pick(isEnglish(), QObject::tr("运行诊断"), QObject::tr("Diagnostics")));
    if (m_settingsAction != nullptr) {
        m_settingsAction->setEnabled(m_facade->settings().systemSettingsEnabled);
    }
    m_filterPanel->setEnglish(isEnglish());
    m_summaryPage->refreshTexts(isEnglish());
    m_badStatPage->refreshTexts(isEnglish());
    m_boardRecordPage->refreshTexts(isEnglish());
    m_pointRecordPage->refreshTexts(isEnglish());
    m_pageTitleLabel->setText(pageTitleFor(pageIdForRow(m_navigation->currentRow())));
    updateFilterOptionsUiState(m_filterOptionsLoadState);
    updateWorkspaceSummary();
}

void DashboardWidget::applyLanguage() {
    StatusText::setLanguage(m_uiSettings.ui.language == LaserSpc::Infrastructure::AppLanguage::English ? StatusText::Language::English : StatusText::Language::Chinese);
}

void DashboardWidget::applyTheme() {
    setStyleSheet(UiTheme::applicationStyleSheet(m_uiSettings.ui.themeStyle));
    if (m_configButton != nullptr && m_configButton->menu() != nullptr) {
        UiTheme::applyPopupMenu(m_configButton->menu());
    }
}

void DashboardWidget::prepareDialog(QWidget* dialog) const {
    if (dialog != nullptr) dialog->setStyleSheet(styleSheet());
}

QString DashboardWidget::pageTitleFor(LaserSpc::Domain::PageId pageId) const {
    switch (pageId) {
        case LaserSpc::Domain::PageId::BadStat: return pick(isEnglish(), QObject::tr("不良统计"), QObject::tr("Bad Statistics"));
        case LaserSpc::Domain::PageId::BoardRecord: return pick(isEnglish(), QObject::tr("单板记录"), QObject::tr("Board Records"));
        case LaserSpc::Domain::PageId::PointRecord: return pick(isEnglish(), QObject::tr("点位记录"), QObject::tr("Point Records"));
        default: return pick(isEnglish(), QObject::tr("数据总览"), QObject::tr("Overview"));
    }
}

bool DashboardWidget::isEnglish() const { return m_uiSettings.ui.language == LaserSpc::Infrastructure::AppLanguage::English; }

void DashboardWidget::openSettingsDialog() {
    if (!m_facade->settings().systemSettingsEnabled) {
        updateStatus(pick(isEnglish(), QObject::tr("当前已禁用系统设置入口。"), QObject::tr("System settings are currently disabled.")));
        return;
    }
    const auto previousCriteria = m_filterPanel->criteria();
    const auto previousDefault = m_facade->defaultFilter();
    SettingsDialog dialog(m_configService.settings(), this);
    prepareDialog(&dialog);
    connect(&dialog, &SettingsDialog::dataCleanupFinished, this, [this](const QString& message) {
        refreshFilterOptions();
        reloadCurrentPage(m_filterPanel->criteria());
        updateStatus(message);
    });
    if (dialog.exec() != QDialog::Accepted) return;
    auto settings = dialog.settings();
    const auto persistedSettings = m_configService.settings();
    settings.mes = persistedSettings.mes;
    settings.allowMockFallback = persistedSettings.allowMockFallback;
    settings.exportDirectory = persistedSettings.exportDirectory;
    settings.systemSettingsEnabled = persistedSettings.systemSettingsEnabled;
    settings.exportReportEnabled = persistedSettings.exportReportEnabled;
    QString errorMessage;
    if (!m_configService.saveSettings(settings, &errorMessage)) {
        LaserSpc::Infrastructure::Logger::error("Failed to save settings: " + errorMessage);
        updateStatus(pick(isEnglish(), QObject::tr("系统设置保存失败："), QObject::tr("Failed to save settings: ")) + errorMessage);
        return;
    }
    m_configService = LaserSpc::Infrastructure::AppConfigService();
    m_uiSettings = m_configService.settings();
    if (m_settingsAction != nullptr) {
        m_settingsAction->setEnabled(m_facade->settings().systemSettingsEnabled);
    }
    applyLanguage();
    applyTheme();
    refreshRuntimeStatus();
    auto buildResult = LaserSpc::Infrastructure::RepositoryFactory::build(settings);
    if (!buildResult.warningMessage.isEmpty()) LaserSpc::Infrastructure::Logger::warn(buildResult.warningMessage);
    m_facade->replaceRepository(std::move(buildResult.repository), buildResult.dataSourceMode, settings);
    const auto nextDefault = m_facade->defaultFilter();
    const bool followDefaultRange = matchesDefault(previousCriteria, previousDefault);
    const auto criteriaToApply = followDefaultRange ? nextDefault : previousCriteria;
    m_filterPanel->setDefaultCriteria(nextDefault);
    m_filterPanel->setCriteria(criteriaToApply);
    refreshFilterOptions();
    applyAutoRefreshSettings();
    refreshShellTexts();
    reloadCurrentPage(criteriaToApply);
    const QString refreshSummary = settings.autoRefreshEnabled ? pick(isEnglish(), QObject::tr("自动刷新 %1 秒").arg(settings.autoRefreshIntervalSeconds), QObject::tr("Auto refresh %1 s").arg(settings.autoRefreshIntervalSeconds)) : pick(isEnglish(), QObject::tr("自动刷新已关闭"), QObject::tr("Auto refresh disabled"));
    const QString criteriaSummary = followDefaultRange ? pick(isEnglish(), QObject::tr("筛选时间已同步更新为新的默认范围。"), QObject::tr("Filter range was synced to the new default window.")) : pick(isEnglish(), QObject::tr("已保留当前手动筛选范围。"), QObject::tr("The current manual filter range was preserved."));
    const QString status = buildResult.warningMessage.isEmpty()
                               ? pick(isEnglish(), QObject::tr("系统设置已保存并生效。"), QObject::tr("Settings saved and applied."))
                               : pick(isEnglish(),
                                      QObject::tr("系统设置已保存，但数据源存在告警。"),
                                      QObject::tr("Settings saved, but the data source reported a warning."));
    updateStatus(status + QObject::tr(" ") + criteriaSummary + QObject::tr(" ") + refreshSummary);
}

void DashboardWidget::openDiagnosticsDialog() {
    const auto snapshot = LaserSpc::Infrastructure::RuntimeDiagnostics::collectSnapshot(m_configService.configFilePath(), LaserSpc::Infrastructure::ExportService::defaultExportDirectory());
    DiagnosticsDialog dialog(snapshot, m_facade->settings().exportReportEnabled, this);
    prepareDialog(&dialog);
    dialog.exec();
}

void DashboardWidget::openMesConfigDialog() {
    MesConfigDialog dialog(m_uiSettings.mes, isEnglish(), this);
    prepareDialog(&dialog);
    if (dialog.exec() != QDialog::Accepted) return;
    auto settings = m_configService.settings();
    settings.mes = dialog.settings();
    settings.ui = m_uiSettings.ui;
    QString errorMessage;
    if (!m_configService.saveSettings(settings, &errorMessage)) {
        updateStatus(pick(isEnglish(), QObject::tr("MES 配置保存失败："), QObject::tr("Failed to save MES settings: ")) + errorMessage);
        return;
    }
    m_configService = LaserSpc::Infrastructure::AppConfigService();
    m_uiSettings = m_configService.settings();
    refreshRuntimeStatus();
    refreshShellTexts();
    updateStatus(pick(isEnglish(), QObject::tr("MES 配置已保存。"), QObject::tr("MES settings saved.")));
}

}  // namespace LaserSpc::Ui
