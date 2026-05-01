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

struct BoardRecordQueryTaskResult {
    LaserSpc::Domain::PageResult<LaserSpc::Domain::BoardRecordRow> data;
    QString repositoryError;
    quint64 requestId = 0;
};

struct BoardRecordExportTaskResult {
    bool success = false;
    QString outputPath;
    QString errorMessage;
};

class BoardRecordPage : public BasePage {
    Q_OBJECT

public:
    explicit BoardRecordPage(LaserSpc::App::AppServiceFacade* facade, QWidget* parent = nullptr);

    LaserSpc::Domain::PageId pageId() const override;
    QString pageTitle() const override;
    void reload(const LaserSpc::Domain::FilterCriteria& criteria) override;
    void refreshTexts(bool english) override;
    QList<PageInsightMetric> pageInsights(bool english) const override;

signals:
    void statusMessageChanged(const QString& message);
    void pointDrillDownRequested(const LaserSpc::Domain::FilterCriteria& criteria, const QString& message);

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
    void exportCurrentQueryToCsv();
    void exportCurrentPageScreenshot();
    LaserSpc::Domain::BoardRecordQuery buildQueryForCurrentView() const;

    QLabel* m_hintLabel = nullptr;
    QGroupBox* m_resultBox = nullptr;
    QGroupBox* m_lineBox = nullptr;
    ChartViewType* m_resultChartView = nullptr;
    ChartViewType* m_resultShareChartView = nullptr;
    ChartViewType* m_lineChartView = nullptr;
    QTableWidget* m_table = nullptr;
    QWidget* m_exportContentWidget = nullptr;
    ResultToolbar* m_resultToolbar = nullptr;
    ExportProgressDialog* m_exportProgressDialog = nullptr;

    LaserSpc::Domain::FilterCriteria m_currentCriteria;
    LaserSpc::Domain::SortOption m_currentSort{"eventTime", Qt::DescendingOrder};
    LaserSpc::Domain::PageResult<LaserSpc::Domain::BoardRecordRow> m_currentResult;
    QFutureWatcher<BoardRecordQueryTaskResult>* m_queryWatcher = nullptr;
    QFutureWatcher<BoardRecordExportTaskResult>* m_exportWatcher = nullptr;
    QuerySequenceGate m_querySequence;
};

}  // namespace LaserSpc::Ui
