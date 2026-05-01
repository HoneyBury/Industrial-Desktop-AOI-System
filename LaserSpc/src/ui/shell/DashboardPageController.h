#pragma once

#include <QObject>
#include <QFutureWatcher>
#include <QStackedWidget>
#include <QTimer>

#include "app/AppServiceFacade.h"
#include "ui/pages/PageAsyncSupport.h"

namespace LaserSpc::Ui {

class BasePage;
class BadStatPage;
class BoardRecordPage;
class FilterPanel;
class PointRecordPage;
class SummaryPage;

struct FilterOptionsTaskResult {
    LaserSpc::Domain::FilterOptions options;
    QString repositoryError;
    LaserSpc::Domain::FilterCriteria criteriaSnapshot;
    quint64 requestId = 0;
};

class DashboardPageController : public QObject {
    Q_OBJECT

public:
    DashboardPageController(LaserSpc::App::AppServiceFacade* facade,
                            FilterPanel* filterPanel,
                            QStackedWidget* stack,
                            SummaryPage* summaryPage,
                            BadStatPage* badStatPage,
                            BoardRecordPage* boardRecordPage,
                            PointRecordPage* pointRecordPage,
                            QTimer* autoRefreshTimer,
                            QFutureWatcher<FilterOptionsTaskResult>* filterOptionsWatcher,
                            QObject* parent = nullptr);

    void refreshData();
    void refreshFilterOptions();
    void handleFilterOptionsFinished();
    void reloadCurrentPage(const LaserSpc::Domain::FilterCriteria& criteria);
    void navigateToPage(LaserSpc::Domain::PageId pageId,
                        const LaserSpc::Domain::FilterCriteria& criteria,
                        const QString& statusMessage);
    void applyAutoRefreshSettings();
    int indexForPageId(LaserSpc::Domain::PageId pageId) const;
    BasePage* currentPage() const;

signals:
    void statusMessageChanged(const QString& message);
    void pageChanged(LaserSpc::Domain::PageId pageId);
    void pageReloaded();
    void filterOptionsStateChanged(PageLoadState state, const QString& detail);
    void filterOptionsApplied(const LaserSpc::Domain::FilterOptions& options,
                              const LaserSpc::Domain::FilterCriteria& criteriaSnapshot,
                              bool enabled);
    void layoutRefreshRequested();
    void workspaceSummaryRefreshRequested();

private:
    void startFilterOptionsRefresh(quint64 requestId);
    QString validateCriteria(const LaserSpc::Domain::FilterCriteria& criteria) const;

    LaserSpc::App::AppServiceFacade* m_facade = nullptr;
    FilterPanel* m_filterPanel = nullptr;
    QStackedWidget* m_stack = nullptr;
    SummaryPage* m_summaryPage = nullptr;
    BadStatPage* m_badStatPage = nullptr;
    BoardRecordPage* m_boardRecordPage = nullptr;
    PointRecordPage* m_pointRecordPage = nullptr;
    QTimer* m_autoRefreshTimer = nullptr;
    QFutureWatcher<FilterOptionsTaskResult>* m_filterOptionsWatcher = nullptr;
    QuerySequenceGate m_filterOptionsSequence;
    PageLoadState m_filterOptionsLoadState = PageLoadState::Idle;
};

}  // namespace LaserSpc::Ui
