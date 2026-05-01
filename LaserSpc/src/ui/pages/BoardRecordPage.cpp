#include "ui/pages/BoardRecordPage.h"
#include <QVariant>

#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QObject>
#include <QSplitter>
#include <QToolTip>
#include <QVBoxLayout>

#include "infrastructure/ExportService.h"
#include "ui/common/StatusText.h"
#include "ui/common/UiTheme.h"
#include "ui/dialogs/ExportPanelDialog.h"
#include "ui/dialogs/ExportProgressDialog.h"
#include "ui/pages/PageContainer.h"
#include "ui/pages/PageSortSupport.h"
#include "ui/pages/TableRenderSupport.h"
#include "ui/widgets/ResultToolbar.h"

#include <algorithm>

namespace LaserSpc::Ui {

namespace {

QString textFor(bool english, const char* chinese, const char* englishText) {
    return english ? QObject::tr(englishText) : QObject::tr(chinese);
}

const QString kStatusSubject = QObject::tr("单板记录页");
const QString kExportSubject = QObject::tr("单板记录");

enum class BoardRecordSortKey {
    ProgramName,
    BoardCode,
    Result,
    LineName,
    DeviceName,
    OperatorName,
    EventTime
};

constexpr std::array<std::pair<int, BoardRecordSortKey>, 7> kSortMapping{{
    {0, BoardRecordSortKey::ProgramName},
    {1, BoardRecordSortKey::BoardCode},
    {2, BoardRecordSortKey::Result},
    {3, BoardRecordSortKey::LineName},
    {4, BoardRecordSortKey::DeviceName},
    {5, BoardRecordSortKey::OperatorName},
    {6, BoardRecordSortKey::EventTime},
}};

QString sortFieldName(BoardRecordSortKey key) {
    switch (key) {
        case BoardRecordSortKey::ProgramName: return QStringLiteral("programName");
        case BoardRecordSortKey::BoardCode: return QStringLiteral("boardCode");
        case BoardRecordSortKey::Result: return QStringLiteral("result");
        case BoardRecordSortKey::LineName: return QStringLiteral("lineName");
        case BoardRecordSortKey::DeviceName: return QStringLiteral("deviceName");
        case BoardRecordSortKey::OperatorName: return QStringLiteral("operatorName");
        case BoardRecordSortKey::EventTime:
        default: return QStringLiteral("eventTime");
    }
}

struct NamedCountBucket {
    QString label;
    int count = 0;
    QStringList sourceLabels;
};

QString previewList(const QStringList& values, int limit = 3) {
    if (values.isEmpty()) {
        return {};
    }
    QStringList preview;
    for (int index = 0; index < values.size() && index < limit; ++index) {
        preview.append(values.at(index));
    }
    if (values.size() > limit) {
        preview.append(QObject::tr("..."));
    }
    return preview.join(QObject::tr("、"));
}

QList<NamedCountBucket> aggregateNamedCounts(const QMap<QString, int>& counts, bool english) {
    QList<NamedCountBucket> buckets;
    QList<QPair<QString, int>> entries;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        entries.append(qMakePair(it.key(), it.value()));
    }
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return left.second > right.second; });

    constexpr int kVisibleCategoryLimit = 6;
    const int directCount = entries.size() > kVisibleCategoryLimit ? kVisibleCategoryLimit - 1 : entries.size();
    for (int index = 0; index < directCount; ++index) {
        NamedCountBucket bucket;
        bucket.label = entries.at(index).first;
        bucket.count = entries.at(index).second;
        bucket.sourceLabels.append(entries.at(index).first);
        buckets.append(bucket);
    }
    if (entries.size() > kVisibleCategoryLimit) {
        NamedCountBucket other;
        other.label = english ? QObject::tr("Others") : QObject::tr("其他");
        for (int index = directCount; index < entries.size(); ++index) {
            other.count += entries.at(index).second;
            other.sourceLabels.append(entries.at(index).first);
        }
        other.label += english ? QObject::tr(" (%1)").arg(other.sourceLabels.size())
                               : QObject::tr("（%1项）").arg(other.sourceLabels.size());
        buckets.append(other);
    }
    return buckets;
}

}  // namespace

BoardRecordPage::BoardRecordPage(LaserSpc::App::AppServiceFacade* facade, QWidget* parent)
    : BasePage(facade, parent) {
    setupUi();
    m_exportProgressDialog = new ExportProgressDialog(this);
    m_queryWatcher = new QFutureWatcher<BoardRecordQueryTaskResult>(this);
    m_exportWatcher = new QFutureWatcher<BoardRecordExportTaskResult>(this);
    connect(m_queryWatcher, &QFutureWatcher<BoardRecordQueryTaskResult>::finished, this, &BoardRecordPage::handleQueryFinished);
    connect(m_exportWatcher, &QFutureWatcher<BoardRecordExportTaskResult>::finished, this, &BoardRecordPage::handleExportFinished);
}

LaserSpc::Domain::PageId BoardRecordPage::pageId() const {
    return LaserSpc::Domain::PageId::BoardRecord;
}

QString BoardRecordPage::pageTitle() const {
    return textFor(uiEnglish(), "单板记录", "Board Records");
}

void BoardRecordPage::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    const auto container = buildScrollablePageContainer(this, rootLayout);
    auto* layout = container.contentLayout;
    m_exportContentWidget = container.contentWidget;

    m_hintLabel = new QLabel(tr("单板记录支持按结果、线体和设备进行分组查看，可双击进入点位记录。"), m_exportContentWidget);
    m_hintLabel->setProperty("hint", QVariant(true));
    m_hintLabel->setWordWrap(true);

    auto* chartSplitter = new QSplitter(Qt::Horizontal, m_exportContentWidget);
    chartSplitter->setChildrenCollapsible(false);

    m_resultBox = new QGroupBox(tr("结果分布"), m_exportContentWidget);
    UiTheme::applyPanel(m_resultBox);
    auto* resultLayout = new QVBoxLayout(m_resultBox);
    auto* resultSplitter = new QSplitter(Qt::Horizontal, m_resultBox);
    resultSplitter->setChildrenCollapsible(false);
    m_resultChartView = new ChartViewType(resultSplitter);
    prepareChartView(m_resultChartView);
    m_resultShareChartView = new ChartViewType(resultSplitter);
    prepareChartView(m_resultShareChartView);
    m_resultShareChartView->setMinimumWidth(240);
    resultSplitter->addWidget(m_resultChartView);
    resultSplitter->addWidget(m_resultShareChartView);
    resultSplitter->setStretchFactor(0, 3);
    resultSplitter->setStretchFactor(1, 2);
    resultLayout->addWidget(resultSplitter);

    m_lineBox = new QGroupBox(tr("线体分布"), m_exportContentWidget);
    UiTheme::applyPanel(m_lineBox);
    auto* lineLayout = new QVBoxLayout(m_lineBox);
    m_lineChartView = new ChartViewType(m_lineBox);
    prepareChartView(m_lineChartView);
    lineLayout->addWidget(m_lineChartView);

    chartSplitter->addWidget(m_resultBox);
    chartSplitter->addWidget(m_lineBox);
    chartSplitter->setStretchFactor(0, 1);
    chartSplitter->setStretchFactor(1, 1);

    m_resultToolbar = new ResultToolbar(tr("导出 CSV"), tr("导出截图"), m_exportContentWidget);
    m_table = new QTableWidget(m_exportContentWidget);
    m_table->setMinimumHeight(540);
    configureDataTable(m_table,
                       {tr("程序名"),
                        tr("板号"),
                        tr("结果"),
                        tr("线体"),
                        tr("设备"),
                        tr("操作员"),
                        tr("时间")},
                       0);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);

    layout->addWidget(m_hintLabel);
    chartSplitter->setSizes({500, 400});
    layout->addWidget(m_resultToolbar);
    layout->addWidget(m_table, 10);
    layout->addWidget(chartSplitter, 2);

    connect(m_resultToolbar, &ResultToolbar::pageSizeChanged, this, [this](int) { queryAndRender(true); });
    connect(m_resultToolbar, &ResultToolbar::previousPageRequested, this, [this]() {
        if (m_currentResult.page > 1) {
            --m_currentResult.page;
            queryAndRender(false);
        }
    });
    connect(m_resultToolbar, &ResultToolbar::nextPageRequested, this, [this]() {
        const int totalPages =
            m_currentResult.pageSize <= 0 ? 1 : qMax(1, (m_currentResult.total + m_currentResult.pageSize - 1) / m_currentResult.pageSize);
        if (m_currentResult.page < totalPages) {
            ++m_currentResult.page;
            queryAndRender(false);
        }
    });
    connect(m_resultToolbar, &ResultToolbar::pageJumpRequested, this, [this](int page) {
        m_currentResult.page = qMax(1, page);
        queryAndRender(false);
    });
    connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int column) {
        const auto key = sortKeyForColumn(column, kSortMapping);
        if (!key.has_value()) {
            return;
        }
        const QString field = sortFieldName(*key);
        if (m_currentSort.field == field) {
            m_currentSort.order = m_currentSort.order == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
        } else {
            m_currentSort.field = field;
            m_currentSort.order = Qt::AscendingOrder;
        }
        m_table->horizontalHeader()->setSortIndicator(column, m_currentSort.order);
        queryAndRender(true);
    });
    connect(m_resultToolbar, &ResultToolbar::exportCsvRequested, this, &BoardRecordPage::exportCurrentQueryToCsv);
    connect(m_resultToolbar, &ResultToolbar::exportImageRequested, this, &BoardRecordPage::exportCurrentPageScreenshot);
    connect(m_resultToolbar, &ResultToolbar::exportPanelRequested, this, [this]() {
        QStringList criteriaSummary{
            tr("时间范围：%1 至 %2")
                .arg(m_currentCriteria.beginTime.toString("yyyy-MM-dd HH:mm:ss"),
                     m_currentCriteria.endTime.toString("yyyy-MM-dd HH:mm:ss")),
            tr("线体：%1").arg(m_currentCriteria.lineName),
            tr("程序：%1").arg(m_currentCriteria.programName),
            tr("设备：%1").arg(m_currentCriteria.deviceName),
            tr("关键字：%1").arg(m_currentCriteria.keyword.isEmpty() ? tr("全部") : m_currentCriteria.keyword)
        };
        QStringList metricSummary{
            tr("总记录：%1").arg(m_currentResult.total),
            tr("当前页：%1").arg(m_currentResult.page),
            tr("排序字段：%1").arg(m_currentSort.field)
        };
        ExportPanelDialog dialog(m_resultToolbar->taskState(),
                                 QStringLiteral("board_record_report"),
                                 QStringLiteral("board_records"),
                                 criteriaSummary,
                                 metricSummary,
                                 this);
        dialog.exec();
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row < 0 || row >= m_currentResult.rows.size()) {
            return;
        }

        const auto& item = m_currentResult.rows.at(row);
        auto criteria = m_currentCriteria;
        criteria.lineName = item.lineName;
        criteria.programName = item.programName;
        criteria.deviceName = item.deviceName;
        criteria.keyword = item.boardCode;
        emit pointDrillDownRequested(
            criteria,
            tr("已从单板记录钻取到点位记录：板号 %1。").arg(item.boardCode));
    });
}

void BoardRecordPage::reload(const LaserSpc::Domain::FilterCriteria& criteria) {
    m_currentCriteria = criteria;
    queryAndRender(true);
}

QList<PageInsightMetric> BoardRecordPage::pageInsights(bool english) const {
    QMap<QString, int> resultCounts;
    QMap<QString, int> lineCounts;
    const LaserSpc::Domain::BoardRecordRow* latestRow = nullptr;
    for (const auto& row : m_currentResult.rows) {
        resultCounts[row.result] += 1;
        lineCounts[row.lineName] += 1;
        if (latestRow == nullptr || row.eventTime > latestRow->eventTime) {
            latestRow = &row;
        }
    }

    const QString pending = english ? QObject::tr("Waiting") : QObject::tr("待加载");
    auto dominantResult = resultCounts.cend();
    for (auto it = resultCounts.cbegin(); it != resultCounts.cend(); ++it) {
        if (dominantResult == resultCounts.cend() || it.value() > dominantResult.value()) {
            dominantResult = it;
        }
    }
    auto hottestLine = lineCounts.cend();
    for (auto it = lineCounts.cbegin(); it != lineCounts.cend(); ++it) {
        if (hottestLine == lineCounts.cend() || it.value() > hottestLine.value()) {
            hottestLine = it;
        }
    }
    const int ngCount = resultCounts.value(QObject::tr("NG"));
    const qreal ngRate = m_currentResult.total == 0 ? 0.0 : static_cast<qreal>(ngCount) * 100.0 / static_cast<qreal>(m_currentResult.total);
    return {
        {english ? QObject::tr("Record volume") : QObject::tr("记录总量"),
         m_currentResult.total == 0 ? pending
                                    : (english ? QObject::tr("%1 rows").arg(m_currentResult.total) : QObject::tr("%1 条").arg(m_currentResult.total))},
        {english ? QObject::tr("NG ratio") : QObject::tr("NG占比"),
         m_currentResult.total == 0 ? pending : QObject::tr("%1% | %2").arg(QString::number(ngRate, 'f', 1)).arg(ngCount)},
        {english ? QObject::tr("Busiest line") : QObject::tr("高频线体"),
         hottestLine == lineCounts.cend() ? pending
                                          : QObject::tr("%1 | %2").arg(hottestLine.key()).arg(hottestLine.value())},
        {english ? QObject::tr("Latest board") : QObject::tr("最新板号"),
         latestRow == nullptr ? pending : QObject::tr("%1 | %2").arg(latestRow->boardCode).arg(latestRow->result)}
    };
}

LaserSpc::Domain::BoardRecordQuery BoardRecordPage::buildQueryForCurrentView() const {
    LaserSpc::Domain::BoardRecordQuery query;
    query.filter = m_currentCriteria;
    query.pagination.page = m_currentResult.page <= 0 ? 1 : m_currentResult.page;
    query.pagination.pageSize = qMax(10, m_resultToolbar->pageSize());
    query.sort = m_currentSort;
    return query;
}

void BoardRecordPage::queryAndRender(bool resetPage) {
    if (resetPage || m_currentResult.page <= 0) {
        m_currentResult.page = 1;
    }

    const quint64 requestId = m_querySequence.registerRequest(isQueryBusy());
    if (requestId == 0) {
        emit statusMessageChanged(StatusText::pagePendingRefresh(kStatusSubject));
        return;
    }

    startQuery(requestId);
}

void BoardRecordPage::startQuery(quint64 requestId) {
    const auto query = buildQueryForCurrentView();
    applyPageState(PageLoadState::Loading, isExportBusy());
    m_hintLabel->setText(tr("正在加载单板记录..."));
    emit statusMessageChanged(StatusText::pageQueryLoading(kStatusSubject));
    runPageTask(m_queryWatcher, [facade = m_facade, query, requestId]() {
        BoardRecordQueryTaskResult result;
        result.requestId = requestId;
        const auto service = facade->recordQueryService();
        result.data = service.queryBoardRecords(query);
        result.repositoryError = service.lastRepositoryError();
        return result;
    });
}

void BoardRecordPage::handleQueryFinished() {
    const auto result = m_queryWatcher->result();
    if (!acceptLatestPageResult(result, m_querySequence, result.requestId, [this](quint64 nextRequestId) {
            startQuery(nextRequestId);
        })) {
        return;
    }

    m_currentResult = result.data;
    m_table->setUpdatesEnabled(false);
    m_table->setRowCount(0);
    m_table->setRowCount(m_currentResult.rows.size());
    for (int row = 0; row < m_currentResult.rows.size(); ++row) {
        const auto& item = m_currentResult.rows.at(row);
        setTextTableRow(m_table,
                        row,
                        {item.programName,
                         item.boardCode,
                         item.result,
                         item.lineName,
                         item.deviceName,
                         item.operatorName,
                         item.eventTime.toString("yyyy-MM-dd HH:mm:ss")});
    }
    m_table->setUpdatesEnabled(true);
    renderCharts();

    const int totalPages =
        m_currentResult.pageSize <= 0 ? 1 : qMax(1, (m_currentResult.total + m_currentResult.pageSize - 1) / m_currentResult.pageSize);
    PageLoadState nextLoadState = PageLoadState::Loaded;
    if (!result.repositoryError.isEmpty()) {
        nextLoadState = PageLoadState::Error;
        m_hintLabel->setText(tr("查询失败：") + result.repositoryError);
        emit statusMessageChanged(StatusText::pageQueryFailed(kStatusSubject, result.repositoryError));
    } else if (m_currentResult.total == 0) {
        nextLoadState = PageLoadState::Empty;
        m_hintLabel->setText(tr("当前筛选条件下暂无单板记录数据。"));
        emit statusMessageChanged(StatusText::pageNoData(kStatusSubject));
    } else {
        m_hintLabel->setText(tr("已加载 %1 条单板记录，当前排序 %2，页码 %3/%4。")
                                 .arg(m_currentResult.total)
                                 .arg(m_currentSort.field)
                                 .arg(m_currentResult.page)
                                 .arg(totalPages));
        emit statusMessageChanged(StatusText::pagedRefresh(kStatusSubject, m_currentResult.page, m_currentResult.total, m_currentSort.field));
    }
    updatePaginationUi();
    applyPageState(nextLoadState, isExportBusy());
}

void BoardRecordPage::renderCharts() {
    QMap<QString, int> resultCounts;
    QMap<QString, int> lineCounts;
    QMap<QString, int> operatorCounts;
    for (const auto& row : m_currentResult.rows) {
        resultCounts[row.result] += 1;
        lineCounts[row.lineName] += 1;
        operatorCounts[row.operatorName.trimmed().isEmpty() ? tr("未填写") : row.operatorName.trimmed()] += 1;
    }

    auto* resultChart = new ChartType();
    styleChart(resultChart, tr("结果分布"));
    auto* resultSeries = new PieSeriesType(resultChart);
    resultSeries->setHoleSize(0.45);
    const QList<QColor> pieColors{chartPalette().teal, chartPalette().amber, chartPalette().blue, chartPalette().coral};
    int colorIndex = 0;
    for (auto it = resultCounts.cbegin(); it != resultCounts.cend(); ++it, ++colorIndex) {
        auto* slice = resultSeries->append(it.key(), it.value());
        const QColor sliceColor = pieColors.at(colorIndex % pieColors.size());
        slice->setColor(sliceColor);
        slice->setLabel(QObject::tr("%1 %2").arg(it.key()).arg(it.value()));
        slice->setLabelVisible(false);
        attachPieSliceHoverBehavior(resultChart,
                                    resultSeries,
                                    slice,
                                    QObject::tr("%1\n数量：%2").arg(it.key()).arg(it.value()),
                                    sliceColor);
        connect(slice, &PieSliceType::clicked, this, [this, key = it.key()]() { selectRowsByTexts(2, {key}); });
    }
    resultChart->addSeries(resultSeries);
    refreshPieLegend(resultChart, resultSeries);
    m_resultChartView->setChart(resultChart);

    const QList<NamedCountBucket> operatorBuckets = aggregateNamedCounts(operatorCounts, uiEnglish());
    auto* shareChart = new ChartType();
    styleChart(shareChart, tr("操作员占比"));
    shareChart->legend()->setVisible(false);
    auto* shareSeries = new PieSeriesType(shareChart);
    shareSeries->setHoleSize(0.55);
    int totalCount = 0;
    for (const auto& bucket : operatorBuckets) {
        totalCount += bucket.count;
    }
    colorIndex = 0;
    for (const auto& bucket : operatorBuckets) {
        auto* slice = shareSeries->append(bucket.label, bucket.count);
        const QColor sliceColor = pieColors.at(colorIndex % pieColors.size());
        slice->setColor(sliceColor);
        slice->setLabelVisible(false);
        const qreal ratio = totalCount == 0 ? 0.0 : static_cast<qreal>(bucket.count) * 100.0 / static_cast<qreal>(totalCount);
        attachPieSliceHoverBehavior(shareChart,
                                    shareSeries,
                                    slice,
                                    QObject::tr("操作员：%1\n记录数：%2\n占比：%3%")
                                        .arg(bucket.label)
                                        .arg(bucket.count)
                                        .arg(QString::number(ratio, 'f', 1)),
                                    sliceColor);
        connect(slice, &PieSliceType::clicked, this, [this, bucket]() { selectRowsByTexts(5, bucket.sourceLabels); });
        ++colorIndex;
    }
    shareChart->addSeries(shareSeries);
    m_resultShareChartView->setChart(shareChart);

    const QList<NamedCountBucket> lineBuckets = aggregateNamedCounts(lineCounts, uiEnglish());
    auto* lineChart = new ChartType();
    styleChart(lineChart, tr("线体分布"));
    auto* lineSeries = new BarSeriesType(lineChart);
    auto* lineSet = new BarSetType(tr("记录数"));
    lineSet->setColor(chartPalette().blue);
    QStringList categories;
    int maxValue = 0;
    for (const auto& bucket : lineBuckets) {
        categories << bucket.label;
        *lineSet << bucket.count;
        maxValue = qMax(maxValue, bucket.count);
    }
    lineSeries->append(lineSet);
    lineChart->addSeries(lineSeries);

    auto* axisX = new BarCategoryAxisType(lineChart);
    axisX->append(categories);
    styleCategoryAxis(axisX);
    axisX->setLabelsVisible(false);
    lineChart->addAxis(axisX, Qt::AlignBottom);
    lineSeries->attachAxis(axisX);

    auto* axisY = new ValueAxisType(lineChart);
    axisY->setRange(0, qMax(1, maxValue + 1));
    axisY->setLabelFormat(QObject::tr("%d"));
    styleValueAxis(axisY, tr("记录数"));
    lineChart->addAxis(axisY, Qt::AlignLeft);
    lineSeries->attachAxis(axisY);

    connect(lineSet, &BarSetType::hovered, this, [this, lineSet, lineBuckets](bool status, int index) {
        lineSet->setColor(status ? chartPalette().blueHover : chartPalette().blue);
        if (!status || index < 0 || index >= lineBuckets.size()) {
            return;
        }
        const auto& bucket = lineBuckets.at(index);
        QToolTip::showText(QCursor::pos(),
                           QObject::tr("线体：%1\n记录数：%2").arg(bucket.label).arg(bucket.count),
                           m_lineChartView);
    });
    connect(lineSet, &BarSetType::clicked, this, [this, lineBuckets](int index) {
        if (index >= 0 && index < lineBuckets.size() && !lineBuckets.at(index).sourceLabels.isEmpty()) {
            selectRowsByTexts(3, lineBuckets.at(index).sourceLabels);
        }
    });
    m_lineChartView->setChart(lineChart);
}

void BoardRecordPage::selectRowsByTexts(int column, const QStringList& values) {
    if (values.isEmpty()) {
        return;
    }

    m_table->clearSelection();
    bool firstMatchFound = false;
    int selectedCount = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->item(row, column) != nullptr && values.contains(m_table->item(row, column)->text())) {
            m_table->selectRow(row);
            ++selectedCount;
            if (!firstMatchFound) {
                m_table->scrollToItem(m_table->item(row, column));
                firstMatchFound = true;
            }
        }
    }
    if (selectedCount > 0) {
        emit statusMessageChanged(QObject::tr("已高亮 %1 条单板记录，命中 %2 个图表类目。").arg(selectedCount).arg(values.size()));
    }
}

void BoardRecordPage::handleExportFinished() {
    const auto result = m_exportWatcher->result();
    if (!result.success) {
        m_resultToolbar->setTaskState(tr("导出失败：") + result.errorMessage, true);
        m_exportProgressDialog->finishTask(false, tr("导出失败：%1").arg(result.errorMessage));
        emit statusMessageChanged(StatusText::exportFailed(kExportSubject, result.errorMessage));
    } else {
        m_resultToolbar->setTaskState(tr("导出完成，文件：") + result.outputPath, false);
        m_exportProgressDialog->finishTask(true, tr("导出完成：%1").arg(result.outputPath));
        emit statusMessageChanged(StatusText::exportCompleted(kExportSubject, result.outputPath));
    }
    applyPageState(loadState(), false);
}

void BoardRecordPage::applyPageState(PageLoadState loadState, bool exportBusy) {
    setLoadState(loadState);
    setExportBusy(exportBusy);
    m_resultToolbar->setEnabled(!isInteractionLocked());
    m_resultToolbar->setExportControlsEnabled(m_facade->settings().exportReportEnabled && !isInteractionLocked());
    m_table->setEnabled(!isQueryBusy());
}

void BoardRecordPage::updatePaginationUi() {
    m_resultToolbar->setCurrentPage(m_currentResult.page);
    m_resultToolbar->setTotalRows(m_currentResult.total);
}

void BoardRecordPage::exportCurrentQueryToCsv() {
    if (isExportBusy()) {
        emit statusMessageChanged(StatusText::exportBusy(kStatusSubject));
        return;
    }

    LaserSpc::Domain::BoardRecordQuery query;
    query.filter = m_currentCriteria;
    query.pagination.page = 1;
    query.pagination.pageSize = qMax(1, m_currentResult.total);
    query.sort = m_currentSort;

    applyPageState(loadState(), true);
    m_resultToolbar->setTaskState(tr("正在导出单板记录 CSV..."), false);
    m_exportProgressDialog->startTask(tr("导出单板记录"), tr("正在生成 CSV 文件"));
    emit statusMessageChanged(StatusText::exportStarted(kExportSubject, QObject::tr(" CSV ")));
    runPageTask(m_exportWatcher, [facade = m_facade, query]() {
        BoardRecordExportTaskResult result;
        const auto rows = facade->recordQueryService().queryBoardRecords(query).rows;
        result.success = Infrastructure::ExportService::exportBoardRecordsToCsv(rows, &result.outputPath, &result.errorMessage);
        return result;
    });
}

void BoardRecordPage::exportCurrentPageScreenshot() {
    if (isExportBusy()) {
        emit statusMessageChanged(StatusText::exportBusy(kStatusSubject));
        return;
    }

    const QPixmap pixmap = captureModuleSnapshot(m_exportContentWidget, {m_table});
    if (pixmap.isNull()) {
        emit statusMessageChanged(StatusText::exportFailed(kExportSubject + tr("截图"),
                                                           tr("当前页面截图失败")));
        return;
    }

    applyPageState(loadState(), true);
    m_resultToolbar->setTaskState(tr("正在导出单板记录截图..."), false);
    m_exportProgressDialog->startTask(tr("导出单板记录"), tr("正在生成截图文件"));
    emit statusMessageChanged(StatusText::exportStarted(kExportSubject, tr("截图")));
    runPageTask(m_exportWatcher, [pixmap]() {
        BoardRecordExportTaskResult result;
        result.success = Infrastructure::ExportService::exportPixmapScreenshot(
            pixmap, QStringLiteral("board_records_screenshot"), &result.outputPath, &result.errorMessage);
        return result;
    });
}

void BoardRecordPage::refreshTexts(bool english) {
    m_hintLabel->setText(textFor(english,
                                 "单板记录支持按结果、线体和设备进行分组查看，可双击进入点位记录。",
                                 "Board records support grouping by result, line, and device, and double-clicking rows to drill into point records."));
    if (m_resultBox != nullptr) m_resultBox->setTitle(textFor(english, "结果与操作员概览", "Results and Operators"));
    if (m_lineBox != nullptr) m_lineBox->setTitle(textFor(english, "线体分布", "Line Distribution"));
    if (m_resultToolbar != nullptr) {
        m_resultToolbar->setTexts(english,
                                  textFor(english, "导出 CSV", "Export CSV"),
                                  textFor(english, "导出截图", "Export Screenshot"));
    }
    configureDataTable(m_table,
                       {textFor(english, "程序名", "Program"),
                        textFor(english, "板号", "Board Code"),
                        textFor(english, "结果", "Result"),
                        textFor(english, "线体", "Line"),
                        textFor(english, "设备", "Device"),
                        textFor(english, "操作员", "Operator"),
                        textFor(english, "时间", "Time")},
                       0);
    renderCharts();
}

}  // namespace LaserSpc::Ui


