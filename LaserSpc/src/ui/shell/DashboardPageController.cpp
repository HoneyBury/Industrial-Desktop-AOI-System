#include "ui/shell/DashboardPageController.h"

#include <QStackedWidget>
#include <QtGlobal>

#include "infrastructure/Logger.h"
#include "ui/common/StatusText.h"
#include "ui/pages/BasePage.h"
#include "ui/widgets/FilterPanel.h"

namespace LaserSpc::Ui {

DashboardPageController::DashboardPageController(LaserSpc::App::AppServiceFacade* facade,
                                                 FilterPanel* filterPanel,
                                                 QStackedWidget* stack,
                                                 SummaryPage* summaryPage,
                                                 BadStatPage* badStatPage,
                                                 BoardRecordPage* boardRecordPage,
                                                 PointRecordPage* pointRecordPage,
                                                 QTimer* autoRefreshTimer,
                                                 QFutureWatcher<FilterOptionsTaskResult>* filterOptionsWatcher,
                                                 QObject* parent)
    : QObject(parent),
      m_facade(facade),
      m_filterPanel(filterPanel),
      m_stack(stack),
      m_summaryPage(summaryPage),
      m_badStatPage(badStatPage),
      m_boardRecordPage(boardRecordPage),
      m_pointRecordPage(pointRecordPage),
      m_autoRefreshTimer(autoRefreshTimer),
      m_filterOptionsWatcher(filterOptionsWatcher) {}

void DashboardPageController::refreshData() {
    refreshFilterOptions();
    reloadCurrentPage(m_filterPanel->criteria());
}

void DashboardPageController::refreshFilterOptions() {
    const quint64 id = m_filterOptionsSequence.registerRequest(m_filterOptionsLoadState == PageLoadState::Loading);
    if (id == 0) {
        LaserSpc::Infrastructure::Logger::warn("Filter options refresh queued because another request is already running.");
        emit statusMessageChanged(StatusText::filterOptionsQueued());
        return;
    }
    LaserSpc::Infrastructure::Logger::info(QString("Starting filter options refresh requestId=%1").arg(id));
    startFilterOptionsRefresh(id);
}

void DashboardPageController::startFilterOptionsRefresh(quint64 requestId) {
    m_filterOptionsLoadState = PageLoadState::Loading;
    emit filterOptionsApplied({}, m_filterPanel->criteria(), false);
    emit filterOptionsStateChanged(PageLoadState::Loading, {});
    emit statusMessageChanged(StatusText::filterOptionsLoading());
    const auto snapshot = m_filterPanel->criteria();
    runPageTask(m_filterOptionsWatcher, [facade = m_facade, snapshot, requestId]() {
        FilterOptionsTaskResult result;
        result.requestId = requestId;
        const auto service = facade->summaryQueryService();
        result.options = service.filterOptions();
        result.repositoryError = service.lastRepositoryError();
        result.criteriaSnapshot = snapshot;
        return result;
    });
}

void DashboardPageController::handleFilterOptionsFinished() {
    const auto result = m_filterOptionsWatcher->result();
    if (!acceptLatestPageResult(result, m_filterOptionsSequence, result.requestId, [this](quint64 nextId) {
            LaserSpc::Infrastructure::Logger::warn(QString("Filter options request became stale; restarting requestId=%1").arg(nextId));
            startFilterOptionsRefresh(nextId);
        })) {
        return;
    }

    m_filterOptionsLoadState =
        result.repositoryError.isEmpty()
            ? (result.options.lineNames.isEmpty() && result.options.programNames.isEmpty() && result.options.deviceNames.isEmpty()
                   ? PageLoadState::Empty
                   : PageLoadState::Loaded)
            : PageLoadState::Error;

    emit filterOptionsApplied(result.options, result.criteriaSnapshot, true);
    if (!result.repositoryError.isEmpty()) {
        LaserSpc::Infrastructure::Logger::error("Filter options load failed: " + result.repositoryError);
        emit filterOptionsStateChanged(PageLoadState::Error, result.repositoryError);
        emit statusMessageChanged(StatusText::filterOptionsFailed(result.repositoryError));
    } else {
        emit filterOptionsStateChanged(m_filterOptionsLoadState, {});
        emit statusMessageChanged(StatusText::filterOptionsLoaded());
    }
}

void DashboardPageController::reloadCurrentPage(const LaserSpc::Domain::FilterCriteria& criteria) {
    const QString error = validateCriteria(criteria);
    if (!error.isEmpty()) {
        LaserSpc::Infrastructure::Logger::warn("Rejected page reload because filter criteria are invalid.");
        emit statusMessageChanged(error);
        return;
    }
    if (auto* page = currentPage()) {
        page->reload(criteria);
        emit pageReloaded();
        emit workspaceSummaryRefreshRequested();
    }
}

void DashboardPageController::navigateToPage(LaserSpc::Domain::PageId pageId,
                                             const LaserSpc::Domain::FilterCriteria& criteria,
                                             const QString& statusMessage) {
    m_filterPanel->setCriteria(criteria);
    const int target = indexForPageId(pageId);
    if (m_stack->currentIndex() != target) {
        m_stack->setCurrentIndex(target);
    }
    emit pageChanged(pageId);
    emit layoutRefreshRequested();
    reloadCurrentPage(criteria);
    if (!statusMessage.isEmpty()) {
        emit statusMessageChanged(statusMessage);
    }
}

void DashboardPageController::applyAutoRefreshSettings() {
    const auto settings = m_facade->settings();
    if (!settings.autoRefreshEnabled) {
        m_autoRefreshTimer->stop();
        emit workspaceSummaryRefreshRequested();
        return;
    }
    m_autoRefreshTimer->start(qMax(5, settings.autoRefreshIntervalSeconds) * 1000);
    emit workspaceSummaryRefreshRequested();
}

int DashboardPageController::indexForPageId(LaserSpc::Domain::PageId pageId) const {
    switch (pageId) {
        case LaserSpc::Domain::PageId::BadStat: return 1;
        case LaserSpc::Domain::PageId::BoardRecord: return 2;
        case LaserSpc::Domain::PageId::PointRecord: return 3;
        default: return 0;
    }
}

BasePage* DashboardPageController::currentPage() const {
    return static_cast<BasePage*>(m_stack->currentWidget());
}

QString DashboardPageController::validateCriteria(const LaserSpc::Domain::FilterCriteria& criteria) const {
    if (criteria.beginTime.isValid() && criteria.endTime.isValid() && criteria.beginTime > criteria.endTime) {
        LaserSpc::Infrastructure::Logger::warn(
            QString("Invalid time range: begin=%1 end=%2").arg(criteria.beginTime.toString(Qt::ISODate)).arg(criteria.endTime.toString(Qt::ISODate)));
        return StatusText::isEnglish()
                   ? QObject::tr("Invalid filters: start time must not be later than end time.")
                   : QObject::tr("查询条件无效：开始时间不能晚于结束时间。");
    }
    return {};
}

}  // namespace LaserSpc::Ui
