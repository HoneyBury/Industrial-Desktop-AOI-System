#pragma once

#include <QFutureWatcher>
class QGroupBox;
#include <QLabel>
#include <QTableWidget>

#include "ui/pages/BasePage.h"
#include "ui/pages/ChartVisuals.h"

namespace LaserSpc::Ui {

class ResultToolbar;
class ExportProgressDialog;

struct PointRecordQueryTaskResult {
    LaserSpc::Domain::PageResult<LaserSpc::Domain::PointRecordRow> data;
    QString repositoryError;
    quint64 requestId = 0;
};

struct PointRecordExportTaskResult {
    bool success = false;
    QString outputPath;
    QString errorMessage;
};

class PointRecordPage : public BasePage {
    Q_OBJECT

public:
    explicit PointRecordPage(LaserSpc::App::AppServiceFacade* facade, QWidget* parent = nullptr);

    LaserSpc::Domain::PageId pageId() const override;
    QString pageTitle() const override;
    void reload(const LaserSpc::Domain::FilterCriteria& criteria) override;
    void refreshTexts(bool english) override;
    QList<PageInsightMetric> pageInsights(bool english) const override;

signals:
    void statusMessageChanged(const QString& message);

private:
    void setupUi();
    void queryAndRender(bool resetPage);
    void startQuery(quint64 requestId);
    void handleQueryFinished();
    void handleExportFinished();
    void applyPageState(PageLoadState loadState, bool exportBusy);
    void updatePaginationUi();
    void renderCharts();
    void selectRowsByTexts(int column, const QStringList& values);
    void showPointDetail(int row);
    void exportCurrentQueryToCsv();
    void exportCurrentPageScreenshot();
    LaserSpc::Domain::PointRecordQuery buildQueryForCurrentView() const;

    QLabel* m_hintLabel = nullptr;
    QLabel* m_readCoverageTitleLabel = nullptr;
    QLabel* m_readCoverageValueLabel = nullptr;
    QLabel* m_laserCoverageTitleLabel = nullptr;
    QLabel* m_laserCoverageValueLabel = nullptr;
    QLabel* m_avgDurationTitleLabel = nullptr;
    QLabel* m_avgDurationValueLabel = nullptr;
    QGroupBox* m_gradeBox = nullptr;
    QGroupBox* m_deviceBox = nullptr;
    ChartViewType* m_gradeChartView = nullptr;
    ChartViewType* m_deviceChartView = nullptr;
    QTableWidget* m_table = nullptr;
    QWidget* m_exportContentWidget = nullptr;
    ResultToolbar* m_resultToolbar = nullptr;
    ExportProgressDialog* m_exportProgressDialog = nullptr;

    LaserSpc::Domain::FilterCriteria m_currentCriteria;
    LaserSpc::Domain::SortOption m_currentSort{"endTime", Qt::DescendingOrder};
    LaserSpc::Domain::PageResult<LaserSpc::Domain::PointRecordRow> m_currentResult;
    QFutureWatcher<PointRecordQueryTaskResult>* m_queryWatcher = nullptr;
    QFutureWatcher<PointRecordExportTaskResult>* m_exportWatcher = nullptr;
    QuerySequenceGate m_querySequence;
};

}  // namespace LaserSpc::Ui
