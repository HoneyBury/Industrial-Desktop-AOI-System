#pragma once

#include <QFutureWatcher>
class QGroupBox;
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
class QFrame;

#include "ui/common/EmptyStateWidget.h"
#include "ui/pages/BasePage.h"
#include "ui/pages/ChartVisuals.h"

namespace LaserSpc::Ui {

class ResultToolbar;
class ExportProgressDialog;

struct MetricCardWidgets {
    QLabel* titleLabel = nullptr;
    QLabel* valueLabel = nullptr;
    QLabel* trendLabel = nullptr;
    QLabel* descLabel = nullptr;
    QProgressBar* accentBar = nullptr;
};

struct SummaryQueryTaskResult {
    LaserSpc::App::SummaryPageData data;
    QString repositoryError;
    quint64 requestId = 0;
};

struct SummaryExportTaskResult {
    bool success = false;
    QString outputPath;
    QString errorMessage;
};

class SummaryPage : public BasePage {
    Q_OBJECT

public:
    explicit SummaryPage(LaserSpc::App::AppServiceFacade* facade, QWidget* parent = nullptr);

    LaserSpc::Domain::PageId pageId() const override;
    QString pageTitle() const override;
    void reload(const LaserSpc::Domain::FilterCriteria& criteria) override;
    void refreshTexts(bool english) override;
    QList<PageInsightMetric> pageInsights(bool english) const override;

signals:
    void statusMessageChanged(const QString& message);
    void boardDrillDownRequested(const LaserSpc::Domain::FilterCriteria& criteria, const QString& message);

private:
    void setupUi();
    QWidget* createMetricCard(MetricCardWidgets& card, const QString& title);
    void queryAndRender(bool resetPage);
    void startQuery(quint64 requestId);
    void handleQueryFinished();
    void handleExportFinished();
    void applyPageState(PageLoadState loadState, bool exportBusy);
    void updatePaginationUi();
    void renderCharts();
    void selectRowsByCategories(const QStringList& categories);
    void updateMetricCards();
    void updateEmptyState(PageLoadState loadState, const QString& detail = QString());
    void exportCurrentQueryToCsv();
    void exportCurrentPageScreenshot();
    LaserSpc::Domain::SummaryQuery buildQueryForCurrentView() const;

    MetricCardWidgets m_totalBoardsCard;
    MetricCardWidgets m_goodBoardsCard;
    MetricCardWidgets m_badBoardsCard;
    MetricCardWidgets m_yieldCard;
    QLabel* m_ngRateTitleLabel = nullptr;
    QLabel* m_ngRateValueLabel = nullptr;
    QLabel* m_activeDevicesTitleLabel = nullptr;
    QLabel* m_activeDevicesValueLabel = nullptr;
    QGroupBox* m_volumeBox = nullptr;
    QGroupBox* m_yieldBox = nullptr;
    ChartViewType* m_volumeChartView = nullptr;
    ChartViewType* m_yieldChartView = nullptr;
    QTableWidget* m_table = nullptr;
    QWidget* m_exportContentWidget = nullptr;
    QLabel* m_summaryLabel = nullptr;
    ResultToolbar* m_resultToolbar = nullptr;
    ExportProgressDialog* m_exportProgressDialog = nullptr;
    EmptyStateWidget* m_emptyStateWidget = nullptr;

    LaserSpc::Domain::FilterCriteria m_currentCriteria;
    LaserSpc::Domain::SortOption m_currentSort{"lastUpdated", Qt::DescendingOrder};
    LaserSpc::Domain::PageResult<LaserSpc::Domain::SummaryRow> m_currentResult;
    QFutureWatcher<SummaryQueryTaskResult>* m_queryWatcher = nullptr;
    QFutureWatcher<SummaryExportTaskResult>* m_exportWatcher = nullptr;
    QuerySequenceGate m_querySequence;
};

}  // namespace LaserSpc::Ui
