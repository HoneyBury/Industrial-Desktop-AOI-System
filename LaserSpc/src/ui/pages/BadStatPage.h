#pragma once

#include <QFutureWatcher>
class QGroupBox;
#include <QLabel>
class QSplitter;
#include <QTableWidget>

#include "ui/pages/BasePage.h"
#include "ui/pages/ChartVisuals.h"

namespace LaserSpc::Ui {

class ResultToolbar;
class ExportProgressDialog;

struct BadStatQueryTaskResult {
    LaserSpc::App::BadStatPageData data;
    QString repositoryError;
    quint64 requestId = 0;
};

struct BadStatExportTaskResult {
    bool success = false;
    QString outputPath;
    QString errorMessage;
};

class BadStatPage : public BasePage {
    Q_OBJECT

public:
    explicit BadStatPage(LaserSpc::App::AppServiceFacade* facade, QWidget* parent = nullptr);

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
    void resizeEvent(QResizeEvent* event) override;
    void startQuery(quint64 requestId);
    void updateBadPointChartsLayout();
    void renderBadPointChart(const QList<LaserSpc::Domain::BadPointStatRow>& rows);
    void renderGradeChart(const QList<LaserSpc::Domain::GradeStatRow>& rows);
    void handleQueryFinished();
    void handleExportFinished();
    void applyPageState(PageLoadState loadState, bool exportBusy);
    void selectBadPointRow(const QString& pointName);
    void selectGradeRow(const QString& grade);
    void drillDownToPointRecordsByBadPoint(const QString& pointName);
    void drillDownToPointRecordsByGrade(const QString& grade);
    void exportCurrentStatsToCsv();
    void exportCurrentPageScreenshot();

    QLabel* m_hintLabel = nullptr;
    QWidget* m_exportContentWidget = nullptr;
    QGroupBox* m_topBox = nullptr;
    QGroupBox* m_gradeBox = nullptr;
    QSplitter* m_badPointChartsSplitter = nullptr;
    ChartViewType* m_badPointChartView = nullptr;
    ChartViewType* m_badPointShareChartView = nullptr;
    ChartViewType* m_gradeChartView = nullptr;
    QTableWidget* m_badPointTable = nullptr;
    QTableWidget* m_gradeTable = nullptr;
    ResultToolbar* m_resultToolbar = nullptr;
    ExportProgressDialog* m_exportProgressDialog = nullptr;
    QLabel* m_totalBadPointsValue = nullptr;
    QLabel* m_totalBadPointsDesc = nullptr;
    QLabel* m_topBadPointValue = nullptr;
    QLabel* m_topBadPointDesc = nullptr;
    QLabel* m_gradeCoverageValue = nullptr;
    QLabel* m_gradeCoverageDesc = nullptr;

    LaserSpc::Domain::FilterCriteria m_currentCriteria;
    QList<LaserSpc::Domain::BadPointStatRow> m_currentBadPoints;
    QList<LaserSpc::Domain::GradeStatRow> m_currentGrades;
    int m_totalBadPoints = 0;
    int m_totalGradePoints = 0;
    QFutureWatcher<BadStatQueryTaskResult>* m_queryWatcher = nullptr;
    QFutureWatcher<BadStatExportTaskResult>* m_exportWatcher = nullptr;
    QuerySequenceGate m_querySequence;
};

}  // namespace LaserSpc::Ui
