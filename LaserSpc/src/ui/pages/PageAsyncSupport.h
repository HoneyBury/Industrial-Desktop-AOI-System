#pragma once

#include <QtConcurrent/QtConcurrentRun>

#include <QFutureWatcher>
#include <QString>
#include <utility>

namespace LaserSpc::Ui {

enum class PageLoadState {
    Idle,
    Loading,
    Loaded,
    Empty,
    Error
};

struct QueryCompletionDecision {
    bool acceptResult = false;
    bool restartLatest = false;
    quint64 restartRequestId = 0;
};

class QuerySequenceGate {
public:
    quint64 registerRequest(bool queryInFlight) {
        ++m_latestRequestId;
        if (queryInFlight) {
            m_pendingRequest = true;
            return 0;
        }

        m_runningRequestId = m_latestRequestId;
        return m_runningRequestId;
    }

    quint64 activateLatestRequest() {
        m_runningRequestId = m_latestRequestId;
        return m_runningRequestId;
    }

    QueryCompletionDecision finish(quint64 completedRequestId) {
        const bool staleResult = completedRequestId != m_latestRequestId;
        if (staleResult || m_pendingRequest) {
            m_pendingRequest = false;
            m_runningRequestId = m_latestRequestId;
            return QueryCompletionDecision{false, true, m_runningRequestId};
        }

        return QueryCompletionDecision{true, false, 0};
    }

    quint64 latestRequestId() const {
        return m_latestRequestId;
    }

    quint64 runningRequestId() const {
        return m_runningRequestId;
    }

    bool hasPendingRequest() const {
        return m_pendingRequest;
    }

private:
    bool m_pendingRequest = false;
    quint64 m_latestRequestId = 0;
    quint64 m_runningRequestId = 0;
};

struct PageViewState {
    PageLoadState loadState = PageLoadState::Idle;
    bool exportBusy = false;
    QString message;
};

template <typename ResultT, typename TaskFunc>
void runPageTask(QFutureWatcher<ResultT>* watcher, TaskFunc&& taskFunc) {
    if (watcher == nullptr) {
        return;
    }

    watcher->setFuture(QtConcurrent::run(std::forward<TaskFunc>(taskFunc)));
}

template <typename ResultT, typename RestartFunc>
bool acceptLatestPageResult(const ResultT& result,
                            QuerySequenceGate& sequenceGate,
                            quint64 requestId,
                            RestartFunc&& restartFunc) {
    const QueryCompletionDecision completion = sequenceGate.finish(requestId);
    if (completion.restartLatest) {
        restartFunc(completion.restartRequestId);
        return false;
    }
    return completion.acceptResult;
}

}  // namespace LaserSpc::Ui
