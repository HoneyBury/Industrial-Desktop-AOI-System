#include "ui/pages/SummaryPage.h"
#include <QVariant>

#include <QGroupBox>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QObject>
#include <QProgressBar>
#include <QSet>
#include <QSplitter>
#include <QToolTip>
#include <QVBoxLayout>
#include <QtMath>

#include "infrastructure/AppConfigService.h"
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

const QString kStatusSubject = QObject::tr("数据总览页");
const QString kExportSubject = QObject::tr("数据总览");

enum class SummarySortKey {
    ProgramName,
    LineName,
    DeviceName,
    TotalBoards,
    GoodBoards,
    BadBoards,
    LastUpdated
};

constexpr std::array<std::pair<int, SummarySortKey>, 7> kSortMapping{{
    {0, SummarySortKey::ProgramName},
    {1, SummarySortKey::LineName},
    {2, SummarySortKey::DeviceName},
    {3, SummarySortKey::TotalBoards},
    {4, SummarySortKey::GoodBoards},
    {5, SummarySortKey::BadBoards},
    {6, SummarySortKey::LastUpdated},
}};

QString sortFieldName(SummarySortKey key) {
    switch (key) {
        case SummarySortKey::ProgramName: return QStringLiteral("programName");
        case SummarySortKey::LineName: return QStringLiteral("lineName");
        case SummarySortKey::DeviceName: return QStringLiteral("deviceName");
        case SummarySortKey::TotalBoards: return QStringLiteral("totalBoards");
        case SummarySortKey::GoodBoards: return QStringLiteral("goodBoards");
        case SummarySortKey::BadBoards: return QStringLiteral("badBoards");
        case SummarySortKey::LastUpdated:
        default: return QStringLiteral("lastUpdated");
    }
}

QStringList buildCriteriaSummary(const LaserSpc::Domain::FilterCriteria& criteria) {
    return {
        QObject::tr("时间范围：%1 至 %2")
            .arg(criteria.beginTime.toString("yyyy-MM-dd HH:mm:ss"), criteria.endTime.toString("yyyy-MM-dd HH:mm:ss")),
        QObject::tr("线体：%1").arg(criteria.lineName),
        QObject::tr("程序：%1").arg(criteria.programName),
        QObject::tr("设备：%1").arg(criteria.deviceName),
        QObject::tr("结果：%1").arg(criteria.result),
        QObject::tr("关键字：%1").arg(criteria.keyword.isEmpty() ? QObject::tr("全部") : criteria.keyword)
    };
}

QString categoryForRow(const LaserSpc::Domain::SummaryRow& row) {
    return QObject::tr("%1/%2").arg(row.lineName, row.programName);
}

QString summarizeDirection(int current, int baseline, const QString& upText, const QString& downText, const QString& flatText) {
    if (current > baseline) {
        return upText.arg(current - baseline);
    }
    if (current < baseline) {
        return downText.arg(baseline - current);
    }
    return flatText;
}

struct SummaryChartBucket {
    QString label;
    int totalBoards = 0;
    int goodBoards = 0;
    int badBoards = 0;
    qreal yieldRate = 0.0;
    QStringList sourceCategories;
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

QString summaryBucketTooltip(const SummaryChartBucket& bucket, const QString& metricLabel) {
    QString tooltip = QObject::tr("%1\n%2：%3\n总板数：%4\n良板数：%5\n不良板数：%6")
                          .arg(bucket.label)
                          .arg(metricLabel)
                          .arg(QString::number(bucket.yieldRate, 'f', 2) + QObject::tr("%"))
                          .arg(bucket.totalBoards)
                          .arg(bucket.goodBoards)
                          .arg(bucket.badBoards);
    if (bucket.sourceCategories.size() > 1) {
        tooltip += QObject::tr("\n归并项：%1").arg(previewList(bucket.sourceCategories));
    }
    return tooltip;
}

QList<SummaryChartBucket> aggregateSummaryBuckets(const LaserSpc::Domain::PageResult<LaserSpc::Domain::SummaryRow>& result,
                                                  bool english) {
    QList<SummaryChartBucket> buckets;
    if (result.rows.isEmpty()) {
        return buckets;
    }

    struct IndexedRow {
        LaserSpc::Domain::SummaryRow row;
        QString category;
    };

    QList<IndexedRow> indexedRows;
    indexedRows.reserve(result.rows.size());
    for (const auto& row : result.rows) {
        indexedRows.append({row, categoryForRow(row)});
    }

    std::sort(indexedRows.begin(), indexedRows.end(), [](const IndexedRow& left, const IndexedRow& right) {
        return left.row.totalBoards > right.row.totalBoards;
    });

    constexpr int kVisibleCategoryLimit = 6;
    const int directCount = indexedRows.size() > kVisibleCategoryLimit ? kVisibleCategoryLimit - 1 : indexedRows.size();
    for (int index = 0; index < directCount; ++index) {
        const auto& item = indexedRows.at(index);
        SummaryChartBucket bucket;
        bucket.label = item.category;
        bucket.totalBoards = item.row.totalBoards;
        bucket.goodBoards = item.row.goodBoards;
        bucket.badBoards = item.row.badBoards;
        bucket.yieldRate = item.row.yieldRate;
        bucket.sourceCategories.append(item.category);
        buckets.append(bucket);
    }

    if (indexedRows.size() > kVisibleCategoryLimit) {
        SummaryChartBucket other;
        other.label = english ? QObject::tr("Others") : QObject::tr("其他");
        for (int index = directCount; index < indexedRows.size(); ++index) {
            const auto& item = indexedRows.at(index);
            other.totalBoards += item.row.totalBoards;
            other.goodBoards += item.row.goodBoards;
            other.badBoards += item.row.badBoards;
            other.sourceCategories.append(item.category);
        }
        other.yieldRate = other.totalBoards == 0 ? 0.0 : static_cast<qreal>(other.goodBoards) * 100.0 / static_cast<qreal>(other.totalBoards);
        other.label += english ? QObject::tr(" (%1)").arg(other.sourceCategories.size())
                               : QObject::tr("（%1项）").arg(other.sourceCategories.size());
        buckets.append(other);
    }

    return buckets;
}

}  // namespace

SummaryPage::SummaryPage(LaserSpc::App::AppServiceFacade* facade, QWidget* parent)
    : BasePage(facade, parent) {
    setupUi();
    m_exportProgressDialog = new ExportProgressDialog(this);
    m_queryWatcher = new QFutureWatcher<SummaryQueryTaskResult>(this);
    m_exportWatcher = new QFutureWatcher<SummaryExportTaskResult>(this);
    connect(m_queryWatcher, &QFutureWatcher<SummaryQueryTaskResult>::finished, this, &SummaryPage::handleQueryFinished);
    connect(m_exportWatcher, &QFutureWatcher<SummaryExportTaskResult>::finished, this, &SummaryPage::handleExportFinished);
}

LaserSpc::Domain::PageId SummaryPage::pageId() const {
    return LaserSpc::Domain::PageId::Summary;
}

QString SummaryPage::pageTitle() const {
    return textFor(uiEnglish(), "数据总览", "Overview");
}

QWidget* SummaryPage::createMetricCard(MetricCardWidgets& card, const QString& title) {
    auto* box = new QGroupBox(title, this);
    UiTheme::applyPanel(box);
    box->setProperty("metricCard", QVariant(true));

    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(6);

    auto* titleLabel = new QLabel(title, box);
    titleLabel->setProperty("metricTitle", QVariant(true));
    titleLabel->setAlignment(Qt::AlignCenter);
    card.titleLabel = titleLabel;

    card.valueLabel = new QLabel(QObject::tr("--"), box);
    card.valueLabel->setProperty("metricValue", QVariant(true));
    card.valueLabel->setAlignment(Qt::AlignCenter);

    card.trendLabel = new QLabel(box);
    card.trendLabel->setProperty("metricTrend", QVariant(true));
    card.trendLabel->setAlignment(Qt::AlignCenter);

    card.descLabel = new QLabel(box);
    card.descLabel->setProperty("metricDesc", QVariant(true));
    card.descLabel->setAlignment(Qt::AlignCenter);
    card.descLabel->setWordWrap(true);

    card.accentBar = new QProgressBar(box);
    card.accentBar->setTextVisible(false);
    card.accentBar->setRange(0, 100);

    layout->addWidget(titleLabel);
    layout->addWidget(card.valueLabel);
    layout->addWidget(card.trendLabel);
    layout->addWidget(card.descLabel);
    layout->addWidget(card.accentBar);
    return box;
}

QFrame* createInsightStripCard(QLabel*& titleLabel, QLabel*& valueLabel, const QString& title, QWidget* parent) {
    auto* box = new QFrame(parent);
    UiTheme::applyPanel(box);
    box->setProperty("compactMetricCard", QVariant(true));
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(4);
    titleLabel = new QLabel(title, box);
    titleLabel->setProperty("metricTitle", QVariant(true));
    valueLabel = new QLabel(QObject::tr("--"), box);
    valueLabel->setProperty("compactMetricValue", QVariant(true));
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    return box;
}

void SummaryPage::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    const auto container = buildScrollablePageContainer(this, rootLayout);
    auto* layout = container.contentLayout;
    m_exportContentWidget = container.contentWidget;

    m_summaryLabel = new QLabel(tr("数据总览会聚合展示线体、程序与设备的产出与良率表现，支持从图表钻取到明细页面。"), m_exportContentWidget);
    m_summaryLabel->setProperty("hint", QVariant(true));
    m_summaryLabel->setWordWrap(true);

    auto* cardLayout = new QHBoxLayout();
    cardLayout->addWidget(createMetricCard(m_totalBoardsCard, tr("总板数")));
    cardLayout->addWidget(createMetricCard(m_goodBoardsCard, tr("良板数")));
    cardLayout->addWidget(createMetricCard(m_badBoardsCard, tr("不良板数")));
    cardLayout->addWidget(createMetricCard(m_yieldCard, tr("良率")));

    auto* insightStripLayout = new QHBoxLayout();
    insightStripLayout->setSpacing(10);
    insightStripLayout->addWidget(createInsightStripCard(m_ngRateTitleLabel, m_ngRateValueLabel, tr("NG 占比"), m_exportContentWidget));
    insightStripLayout->addWidget(createInsightStripCard(m_activeDevicesTitleLabel, m_activeDevicesValueLabel, tr("活跃设备数"), m_exportContentWidget));
    insightStripLayout->addStretch();

    auto* chartSplitter = new QSplitter(Qt::Horizontal, m_exportContentWidget);
    chartSplitter->setChildrenCollapsible(false);

    m_volumeBox = new QGroupBox(tr("线体产出对比"), m_exportContentWidget);
    UiTheme::applyPanel(m_volumeBox);
    auto* volumeLayout = new QVBoxLayout(m_volumeBox);
    m_volumeChartView = new ChartViewType(m_volumeBox);
    prepareChartView(m_volumeChartView);
    volumeLayout->addWidget(m_volumeChartView);

    m_yieldBox = new QGroupBox(tr("良率走势"), m_exportContentWidget);
    UiTheme::applyPanel(m_yieldBox);
    auto* yieldLayout = new QVBoxLayout(m_yieldBox);
    m_yieldChartView = new ChartViewType(m_yieldBox);
    prepareChartView(m_yieldChartView);
    yieldLayout->addWidget(m_yieldChartView);

    chartSplitter->addWidget(m_volumeBox);
    chartSplitter->addWidget(m_yieldBox);
    chartSplitter->setStretchFactor(0, 1);
    chartSplitter->setStretchFactor(1, 1);

    m_resultToolbar = new ResultToolbar(tr("导出 CSV"), tr("导出截图"), m_exportContentWidget);
    m_table = new QTableWidget(m_exportContentWidget);
    m_table->setMinimumHeight(520);
    configureDataTable(m_table,
                       {tr("程序名"),
                        tr("线体"),
                        tr("设备"),
                        tr("总板数"),
                        tr("良板数"),
                        tr("不良板数"),
                        tr("最近更新时间")},
                       0);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);

    m_emptyStateWidget = new EmptyStateWidget(m_exportContentWidget);
    m_emptyStateWidget->setVisible(false);
    m_emptyStateWidget->setActionText(tr("刷新数据"));
    m_emptyStateWidget->setActionVisible(true);

    layout->addWidget(m_summaryLabel);
    layout->addLayout(cardLayout);
    layout->addLayout(insightStripLayout);
    chartSplitter->setSizes({520, 420});
    layout->addWidget(m_resultToolbar);
    layout->addWidget(m_emptyStateWidget);
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
    connect(m_resultToolbar, &ResultToolbar::exportCsvRequested, this, &SummaryPage::exportCurrentQueryToCsv);
    connect(m_resultToolbar, &ResultToolbar::exportImageRequested, this, &SummaryPage::exportCurrentPageScreenshot);
    connect(m_resultToolbar, &ResultToolbar::exportPanelRequested, this, [this]() {
        QStringList metricSummary{
            tr("总板数：%1").arg(m_totalBoardsCard.valueLabel->text()),
            tr("良板数：%1").arg(m_goodBoardsCard.valueLabel->text()),
            tr("不良板数：%1").arg(m_badBoardsCard.valueLabel->text()),
            tr("良率：%1").arg(m_yieldCard.valueLabel->text()),
            tr("总记录：%1 行").arg(m_currentResult.total)
        };
        ExportPanelDialog dialog(m_resultToolbar->taskState(),
                                 QStringLiteral("summary_report"),
                                 QStringLiteral("summary"),
                                 buildCriteriaSummary(m_currentCriteria),
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
        criteria.keyword.clear();
        emit boardDrillDownRequested(
            criteria,
            tr("已从总览钻取到单板记录：%1 / %2 / %3。").arg(item.lineName, item.programName, item.deviceName));
    });
    connect(m_emptyStateWidget, &EmptyStateWidget::actionTriggered, this, [this]() {
        emit statusMessageChanged(StatusText::pagePendingRefresh(kStatusSubject));
    });
}

void SummaryPage::reload(const LaserSpc::Domain::FilterCriteria& criteria) {
    m_currentCriteria = criteria;
    queryAndRender(true);
}

QList<PageInsightMetric> SummaryPage::pageInsights(bool english) const {
    int totalBoards = 0;
    int goodBoards = 0;
    const LaserSpc::Domain::SummaryRow* hottestRow = nullptr;
    const LaserSpc::Domain::SummaryRow* weakestRow = nullptr;
    for (const auto& row : m_currentResult.rows) {
        totalBoards += row.totalBoards;
        goodBoards += row.goodBoards;
        if (hottestRow == nullptr || row.totalBoards > hottestRow->totalBoards) {
            hottestRow = &row;
        }
        if (weakestRow == nullptr || row.yieldRate < weakestRow->yieldRate) {
            weakestRow = &row;
        }
    }

    const QString pending = english ? QObject::tr("Waiting") : QObject::tr("待加载");
    const qreal weightedYield = totalBoards == 0 ? 0.0 : static_cast<qreal>(goodBoards) * 100.0 / static_cast<qreal>(totalBoards);
    return {
        {english ? QObject::tr("Total output") : QObject::tr("总产出"),
         totalBoards == 0 ? pending
                          : (english ? QObject::tr("%1 boards").arg(totalBoards)
                                     : QObject::tr("%1 板").arg(totalBoards))},
        {english ? QObject::tr("Weighted yield") : QObject::tr("综合良率"),
         totalBoards == 0 ? pending : QObject::tr("%1%").arg(QString::number(weightedYield, 'f', 2))},
        {english ? QObject::tr("Output hot spot") : QObject::tr("产出热点"),
         hottestRow == nullptr ? pending
                               : QObject::tr("%1 | %2").arg(categoryForRow(*hottestRow)).arg(hottestRow->totalBoards)},
        {english ? QObject::tr("Yield risk") : QObject::tr("良率风险"),
         weakestRow == nullptr ? pending
                               : QObject::tr("%1 | %2%").arg(categoryForRow(*weakestRow)).arg(QString::number(weakestRow->yieldRate, 'f', 2))}
    };
}

void SummaryPage::refreshTexts(bool english) {
    m_summaryLabel->setText(textFor(english,
                                    "数据总览会聚合展示线体、程序与设备的产出与良率表现，支持从图表钻取到明细页面。",
                                    "Overview aggregates output and yield across lines, programs, and devices, and supports drill-down from charts to detail pages."));
    if (m_totalBoardsCard.titleLabel != nullptr) m_totalBoardsCard.titleLabel->setText(textFor(english, "总板数", "Total boards"));
    if (m_goodBoardsCard.titleLabel != nullptr) m_goodBoardsCard.titleLabel->setText(textFor(english, "良板数", "Good boards"));
    if (m_badBoardsCard.titleLabel != nullptr) m_badBoardsCard.titleLabel->setText(textFor(english, "不良板数", "Bad boards"));
    if (m_yieldCard.titleLabel != nullptr) m_yieldCard.titleLabel->setText(textFor(english, "良率", "Yield"));
    if (m_ngRateTitleLabel != nullptr) m_ngRateTitleLabel->setText(textFor(english, "NG 占比", "NG Ratio"));
    if (m_activeDevicesTitleLabel != nullptr) m_activeDevicesTitleLabel->setText(textFor(english, "活跃设备数", "Active Devices"));
    if (m_volumeBox != nullptr) m_volumeBox->setTitle(textFor(english, "线体产出对比", "Line Output"));
    if (m_yieldBox != nullptr) m_yieldBox->setTitle(textFor(english, "良率走势", "Yield Trend"));
    if (m_resultToolbar != nullptr) {
        m_resultToolbar->setTexts(english,
                                  textFor(english, "导出 CSV", "Export CSV"),
                                  textFor(english, "导出截图", "Export Screenshot"));
    }
    configureDataTable(m_table,
                       {textFor(english, "程序名", "Program"),
                        textFor(english, "线体", "Line"),
                        textFor(english, "设备", "Device"),
                        textFor(english, "总板数", "Total"),
                        textFor(english, "良板数", "Good"),
                        textFor(english, "不良板数", "Bad"),
                        textFor(english, "最近更新时间", "Last Updated")},
                       0);
    m_emptyStateWidget->setActionText(textFor(english, "刷新数据", "Refresh"));
    renderCharts();
}

LaserSpc::Domain::SummaryQuery SummaryPage::buildQueryForCurrentView() const {
    LaserSpc::Domain::SummaryQuery query;
    query.filter = m_currentCriteria;
    query.pagination.page = m_currentResult.page <= 0 ? 1 : m_currentResult.page;
    query.pagination.pageSize = qMax(10, m_resultToolbar->pageSize());
    query.sort = m_currentSort;
    return query;
}

void SummaryPage::queryAndRender(bool resetPage) {
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

void SummaryPage::startQuery(quint64 requestId) {
    const auto query = buildQueryForCurrentView();
    applyPageState(PageLoadState::Loading, isExportBusy());
    updateEmptyState(PageLoadState::Loading);
    m_summaryLabel->setText(tr("正在加载数据总览..."));
    emit statusMessageChanged(StatusText::pageQueryLoading(kStatusSubject));
    runPageTask(m_queryWatcher, [facade = m_facade, query, requestId]() {
        SummaryQueryTaskResult result;
        result.requestId = requestId;
        const auto service = facade->summaryQueryService();
        result.data = service.querySummary(query);
        result.repositoryError = service.lastRepositoryError();
        return result;
    });
}

void SummaryPage::handleQueryFinished() {
    const auto result = m_queryWatcher->result();
    if (!acceptLatestPageResult(result, m_querySequence, result.requestId, [this](quint64 nextRequestId) {
            startQuery(nextRequestId);
        })) {
        return;
    }

    const auto& data = result.data;
    m_currentResult = data.table;

    const QList<QPair<QLabel*, QLabel*>> cards = {
        {m_totalBoardsCard.valueLabel, m_totalBoardsCard.descLabel},
        {m_goodBoardsCard.valueLabel, m_goodBoardsCard.descLabel},
        {m_badBoardsCard.valueLabel, m_badBoardsCard.descLabel},
        {m_yieldCard.valueLabel, m_yieldCard.descLabel}
    };
    for (int index = 0; index < data.metrics.size() && index < cards.size(); ++index) {
        cards.at(index).first->setText(data.metrics.at(index).value);
        cards.at(index).second->setText(data.metrics.at(index).description);
    }

    m_table->setUpdatesEnabled(false);
    m_table->setRowCount(0);
    m_table->setRowCount(data.table.rows.size());
    for (int row = 0; row < data.table.rows.size(); ++row) {
        const auto& item = data.table.rows.at(row);
        setTextTableRow(m_table,
                        row,
                        {item.programName,
                         item.lineName,
                         item.deviceName,
                         QString::number(item.totalBoards),
                         QString::number(item.goodBoards),
                         QString::number(item.badBoards),
                         item.lastUpdated.toString("yyyy-MM-dd HH:mm:ss")});
    }
    m_table->setUpdatesEnabled(true);

    updateMetricCards();
    renderCharts();

    const int totalPages =
        m_currentResult.pageSize <= 0 ? 1 : qMax(1, (m_currentResult.total + m_currentResult.pageSize - 1) / m_currentResult.pageSize);
    PageLoadState nextLoadState = PageLoadState::Loaded;
    if (!result.repositoryError.isEmpty()) {
        nextLoadState = PageLoadState::Error;
        m_summaryLabel->setText(tr("查询失败：") + result.repositoryError);
        emit statusMessageChanged(StatusText::pageQueryFailed(kStatusSubject, result.repositoryError));
    } else if (m_currentResult.total == 0) {
        nextLoadState = PageLoadState::Empty;
        m_summaryLabel->setText(tr("当前筛选条件下暂无汇总数据。"));
        emit statusMessageChanged(StatusText::pageNoData(kStatusSubject));
    } else {
        m_summaryLabel->setText(tr("已加载 %1 条汇总记录，当前排序 %2，页码 %3/%4。")
                                    .arg(m_currentResult.total)
                                    .arg(m_currentSort.field)
                                    .arg(m_currentResult.page)
                                    .arg(totalPages));
        emit statusMessageChanged(StatusText::pagedRefresh(kStatusSubject, m_currentResult.page, m_currentResult.total, m_currentSort.field));
    }
    updatePaginationUi();
    updateEmptyState(nextLoadState, result.repositoryError);
    applyPageState(nextLoadState, isExportBusy());
}

void SummaryPage::renderCharts() {
    const QList<SummaryChartBucket> buckets = aggregateSummaryBuckets(m_currentResult, uiEnglish());
    auto* volumeChart = new ChartType();
    styleChart(volumeChart, tr("线体产出对比"));
    auto* totalSet = new BarSetType(tr("总板数"));
    auto* goodSet = new BarSetType(tr("良板数"));
    auto* badSet = new BarSetType(tr("不良板数"));
    totalSet->setColor(chartPalette().blue);
    goodSet->setColor(chartPalette().teal);
    badSet->setColor(chartPalette().amber);

    QStringList categories;
    int maxValue = 0;
    for (const auto& bucket : buckets) {
        categories << bucket.label;
        *totalSet << bucket.totalBoards;
        *goodSet << bucket.goodBoards;
        *badSet << bucket.badBoards;
        maxValue = qMax(maxValue, bucket.totalBoards);
    }

    auto* volumeSeries = new BarSeriesType(volumeChart);
    volumeSeries->append(totalSet);
    volumeSeries->append(goodSet);
    volumeSeries->append(badSet);
    volumeChart->addSeries(volumeSeries);

    auto* volumeAxisX = new BarCategoryAxisType(volumeChart);
    volumeAxisX->append(categories);
    styleCategoryAxis(volumeAxisX);
    volumeAxisX->setLabelsVisible(false);
    volumeChart->addAxis(volumeAxisX, Qt::AlignBottom);
    volumeSeries->attachAxis(volumeAxisX);

    auto* volumeAxisY = new ValueAxisType(volumeChart);
    volumeAxisY->setRange(0, qMax(1, maxValue + 2));
    volumeAxisY->setLabelFormat(QObject::tr("%d"));
    styleValueAxis(volumeAxisY, tr("板数"));
    volumeChart->addAxis(volumeAxisY, Qt::AlignLeft);
    volumeSeries->attachAxis(volumeAxisY);

    const QList<BarSetType*> volumeSets{totalSet, goodSet, badSet};
    for (BarSetType* set : volumeSets) {
        const QColor baseColor = set->color();
        connect(set, &BarSetType::clicked, this, [this, buckets](int index) {
            if (index >= 0 && index < buckets.size() && !buckets.at(index).sourceCategories.isEmpty()) {
                selectRowsByCategories(buckets.at(index).sourceCategories);
            }
        });
        connect(set, &BarSetType::hovered, this, [this, set, baseColor, buckets](bool status, int index) {
            set->setColor(status ? baseColor.lighter(115) : baseColor);
            if (!status || index < 0 || index >= buckets.size()) {
                return;
            }
            const auto& bucket = buckets.at(index);
            QToolTip::showText(QCursor::pos(),
                               QObject::tr("%1\n总板数：%2\n良板数：%3\n不良板数：%4\n良率：%5%")
                                   .arg(bucket.label)
                                   .arg(bucket.totalBoards)
                                   .arg(bucket.goodBoards)
                                   .arg(bucket.badBoards)
                                   .arg(QString::number(bucket.yieldRate, 'f', 2)),
                               m_volumeChartView);
        });
    }
    m_volumeChartView->setChart(volumeChart);

    auto* yieldChart = new ChartType();
    styleChart(yieldChart, tr("良率走势"));
    auto* yieldSeries = new LineSeriesType(yieldChart);
    yieldSeries->setName(tr("良率"));
    styleLineSeries(yieldSeries, chartPalette().coral);
    qreal maxYield = 0.0;
    for (int index = 0; index < buckets.size(); ++index) {
        const qreal yieldValue = buckets.at(index).yieldRate;
        yieldSeries->append(index, yieldValue);
        maxYield = qMax(maxYield, yieldValue);
    }
    yieldChart->addSeries(yieldSeries);

    auto* yieldAxisX = new BarCategoryAxisType(yieldChart);
    yieldAxisX->append(categories);
    styleCategoryAxis(yieldAxisX);
    yieldAxisX->setLabelsVisible(false);
    yieldChart->addAxis(yieldAxisX, Qt::AlignBottom);
    yieldSeries->attachAxis(yieldAxisX);

    auto* yieldAxisY = new ValueAxisType(yieldChart);
    const qreal yieldAxisMax = qMax<qreal>(100.0, qCeil(maxYield) + 2.0);
    yieldAxisY->setRange(0, yieldAxisMax);
    yieldAxisY->setLabelFormat(QObject::tr("%.0f%%"));
    styleValueAxis(yieldAxisY, tr("良率"));
    yieldChart->addAxis(yieldAxisY, Qt::AlignLeft);
    yieldSeries->attachAxis(yieldAxisY);

    connect(yieldSeries, &LineSeriesType::hovered, this, [this, buckets](const QPointF& point, bool status) {
        if (!status) {
            return;
        }
        const int index = qRound(point.x());
        if (index < 0 || index >= buckets.size()) {
            return;
        }
        const auto& bucket = buckets.at(index);
        QToolTip::showText(QCursor::pos(),
                           QObject::tr("%1\n良率：%2%\n总板数：%3")
                               .arg(bucket.label)
                               .arg(QString::number(bucket.yieldRate, 'f', 2))
                               .arg(bucket.totalBoards),
                           m_yieldChartView);
    });
    m_yieldChartView->setChart(yieldChart);
}

void SummaryPage::selectRowsByCategories(const QStringList& categories) {
    if (categories.isEmpty()) {
        return;
    }

    m_table->clearSelection();
    bool firstMatchFound = false;
    int selectedCount = 0;
    for (int row = 0; row < m_currentResult.rows.size(); ++row) {
        if (categories.contains(categoryForRow(m_currentResult.rows.at(row)))) {
            m_table->selectRow(row);
            ++selectedCount;
            if (!firstMatchFound && m_table->item(row, 0) != nullptr) {
                m_table->scrollToItem(m_table->item(row, 0));
                firstMatchFound = true;
            }
        }
    }
    if (selectedCount > 0) {
        emit statusMessageChanged(QObject::tr("已选中 %1 条汇总记录，关联 %2 个图表类目。").arg(selectedCount).arg(categories.size()));
    }
}

void SummaryPage::updateMetricCards() {
    int totalBoards = 0;
    int goodBoards = 0;
    int badBoards = 0;
    qreal totalYield = 0.0;
    for (const auto& row : m_currentResult.rows) {
        totalBoards += row.totalBoards;
        goodBoards += row.goodBoards;
        badBoards += row.badBoards;
        totalYield += row.yieldRate;
    }

    const int rowCount = qMax(1, m_currentResult.rows.size());
    const int avgBoards = rowCount == 0 ? 0 : totalBoards / rowCount;
    const int avgGoodBoards = rowCount == 0 ? 0 : goodBoards / rowCount;
    const int avgBadBoards = rowCount == 0 ? 0 : badBoards / rowCount;
    const qreal avgYield = rowCount == 0 ? 0.0 : totalYield / rowCount;
    const qreal ngRate = totalBoards == 0 ? 0.0 : static_cast<qreal>(badBoards) * 100.0 / static_cast<qreal>(totalBoards);
    QSet<QString> activeDevices;
    for (const auto& row : m_currentResult.rows) {
        if (!row.deviceName.trimmed().isEmpty()) {
            activeDevices.insert(row.deviceName.trimmed());
        }
    }

    m_totalBoardsCard.trendLabel->setText(summarizeDirection(totalBoards,
                                                             avgBoards,
                                                             tr("高于均值 +%1"),
                                                             tr("低于均值 -%1"),
                                                             tr("基本持平")));
    m_totalBoardsCard.accentBar->setValue(qMin(100, totalBoards == 0 ? 0 : qMax(10, totalBoards * 100 / qMax(1, totalBoards + avgBoards))));

    m_goodBoardsCard.trendLabel->setText(summarizeDirection(goodBoards,
                                                            avgGoodBoards,
                                                            tr("高于均值 +%1"),
                                                            tr("低于均值 -%1"),
                                                            tr("持平")));
    m_goodBoardsCard.accentBar->setValue(totalBoards == 0 ? 0 : qBound(0, goodBoards * 100 / qMax(1, totalBoards), 100));

    m_badBoardsCard.trendLabel->setText(summarizeDirection(badBoards,
                                                           avgBadBoards,
                                                           tr("高于均值 +%1"),
                                                           tr("低于均值 -%1"),
                                                           tr("持平")));
    m_badBoardsCard.accentBar->setValue(totalBoards == 0 ? 0 : qBound(0, badBoards * 100 / qMax(1, totalBoards), 100));

    const qreal yieldValue = m_yieldCard.valueLabel->text().remove('%').toDouble();
    const qreal yieldDelta = yieldValue - avgYield;
    if (yieldDelta > 0.05) {
        m_yieldCard.trendLabel->setText(tr("高于均值 +%1pp").arg(QString::number(yieldDelta, 'f', 1)));
    } else if (yieldDelta < -0.05) {
        m_yieldCard.trendLabel->setText(tr("低于均值 %1pp").arg(QString::number(yieldDelta, 'f', 1)));
    } else {
        m_yieldCard.trendLabel->setText(tr("持平"));
    }
    m_yieldCard.accentBar->setValue(qBound(0, qRound(yieldValue), 100));
    if (m_ngRateValueLabel != nullptr) {
        m_ngRateValueLabel->setText(QObject::tr("%1%").arg(QString::number(ngRate, 'f', 1)));
    }
    if (m_activeDevicesValueLabel != nullptr) {
        m_activeDevicesValueLabel->setText(QString::number(activeDevices.size()));
    }
}

void SummaryPage::updateEmptyState(PageLoadState loadState, const QString& detail) {
    const bool showEmpty = loadState == PageLoadState::Empty || loadState == PageLoadState::Error || loadState == PageLoadState::Loading;
    m_emptyStateWidget->setVisible(showEmpty);
    m_table->setVisible(!showEmpty || loadState == PageLoadState::Loaded);

    switch (loadState) {
        case PageLoadState::Loading:
            m_emptyStateWidget->setTitle(tr("正在加载"));
            m_emptyStateWidget->setDescription(tr("正在根据筛选条件聚合统计，请稍候。"));
            m_emptyStateWidget->setActionText(tr("刷新数据"));
            break;
        case PageLoadState::Error:
            m_emptyStateWidget->setTitle(tr("加载失败"));
            m_emptyStateWidget->setDescription(detail.isEmpty() ? tr("汇总数据加载失败，请检查连接后重试。")
                                                                : tr("错误详情：%1").arg(detail));
            m_emptyStateWidget->setActionText(tr("重新加载"));
            break;
        case PageLoadState::Empty:
            m_emptyStateWidget->setTitle(tr("暂无数据"));
            m_emptyStateWidget->setDescription(tr("当前筛选条件下没有可展示的汇总数据。"));
            m_emptyStateWidget->setActionText(tr("刷新数据"));
            break;
        case PageLoadState::Loaded:
        case PageLoadState::Idle:
        default:
            m_emptyStateWidget->setVisible(false);
            break;
    }
}

void SummaryPage::handleExportFinished() {
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

void SummaryPage::applyPageState(PageLoadState loadState, bool exportBusy) {
    setLoadState(loadState);
    setExportBusy(exportBusy);
    m_resultToolbar->setEnabled(!isInteractionLocked());
    m_resultToolbar->setExportControlsEnabled(m_facade->settings().exportReportEnabled && !isInteractionLocked());
    m_table->setEnabled(!isQueryBusy());
}

void SummaryPage::updatePaginationUi() {
    m_resultToolbar->setCurrentPage(m_currentResult.page);
    m_resultToolbar->setTotalRows(m_currentResult.total);
}

void SummaryPage::exportCurrentQueryToCsv() {
    if (isExportBusy()) {
        emit statusMessageChanged(StatusText::exportBusy(kStatusSubject));
        return;
    }
    LaserSpc::Domain::SummaryQuery query;
    query.filter = m_currentCriteria;
    query.pagination.page = 1;
    query.pagination.pageSize = qMax(1, m_currentResult.total);
    query.sort = m_currentSort;
    applyPageState(loadState(), true);
    m_resultToolbar->setTaskState(tr("正在导出数据总览 CSV..."), false);
    m_exportProgressDialog->startTask(tr("导出数据总览"), tr("正在生成 CSV 文件"));
    emit statusMessageChanged(StatusText::exportStarted(kExportSubject, QObject::tr(" CSV ")));
    runPageTask(m_exportWatcher, [facade = m_facade, query]() {
        SummaryExportTaskResult result;
        const auto rows = facade->summaryQueryService().querySummary(query).table.rows;
        result.success = Infrastructure::ExportService::exportSummaryRowsToCsv(rows, &result.outputPath, &result.errorMessage);
        return result;
    });
}

void SummaryPage::exportCurrentPageScreenshot() {
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
    m_resultToolbar->setTaskState(tr("正在导出数据总览截图..."), false);
    m_exportProgressDialog->startTask(tr("导出数据总览"), tr("正在生成截图文件"));
    emit statusMessageChanged(StatusText::exportStarted(kExportSubject, tr("截图")));
    runPageTask(m_exportWatcher, [pixmap]() {
        SummaryExportTaskResult result;
        result.success = Infrastructure::ExportService::exportPixmapScreenshot(
            pixmap, QObject::tr("summary_screenshot"), &result.outputPath, &result.errorMessage);
        return result;
    });
}

}  // namespace LaserSpc::Ui


