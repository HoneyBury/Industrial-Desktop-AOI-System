#include "ui/pages/PointRecordPage.h"
#include <QVariant>

#include <QGroupBox>
#include <QFrame>
#include <QHeaderView>
#include <QJsonObject>
#include <QSplitter>
#include <QToolTip>
#include <QVBoxLayout>

#include "infrastructure/ExportService.h"
#include "infrastructure/PointDetailJsonService.h"
#include "ui/common/StatusText.h"
#include "ui/common/UiTheme.h"
#include "ui/dialogs/ExportPanelDialog.h"
#include "ui/dialogs/ExportProgressDialog.h"
#include "ui/dialogs/PointDetailDialog.h"
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

QString boolText(bool english, bool value) {
    return value ? textFor(english, "是", "Yes") : textFor(english, "否", "No");
}

const QString kStatusSubject = QObject::tr("点位记录页");
const QString kExportSubject = QObject::tr("点位记录");

enum class PointRecordSortKey {
    ProgramName,
    BoardCode,
    PointName,
    Result,
    ReadGrade,
    LaserContent,
    ReadCodeContent,
    StartTime,
    EndTime,
    DeviceName,
    IsLaser,
    IsReadCode,
    DetailJsonPath
};

constexpr std::array<std::pair<int, PointRecordSortKey>, 13> kSortMapping{{
    {0, PointRecordSortKey::ProgramName},
    {1, PointRecordSortKey::BoardCode},
    {2, PointRecordSortKey::PointName},
    {3, PointRecordSortKey::Result},
    {4, PointRecordSortKey::ReadGrade},
    {5, PointRecordSortKey::LaserContent},
    {6, PointRecordSortKey::ReadCodeContent},
    {7, PointRecordSortKey::StartTime},
    {8, PointRecordSortKey::EndTime},
    {9, PointRecordSortKey::DeviceName},
    {10, PointRecordSortKey::IsLaser},
    {11, PointRecordSortKey::IsReadCode},
    {12, PointRecordSortKey::DetailJsonPath},
}};

QString sortFieldName(PointRecordSortKey key) {
    switch (key) {
        case PointRecordSortKey::ProgramName: return QStringLiteral("programName");
        case PointRecordSortKey::BoardCode: return QStringLiteral("boardCode");
        case PointRecordSortKey::PointName: return QStringLiteral("pointName");
        case PointRecordSortKey::Result: return QStringLiteral("result");
        case PointRecordSortKey::ReadGrade: return QStringLiteral("readGrade");
        case PointRecordSortKey::LaserContent: return QStringLiteral("laserContent");
        case PointRecordSortKey::ReadCodeContent: return QStringLiteral("readCodeContent");
        case PointRecordSortKey::StartTime: return QStringLiteral("startTime");
        case PointRecordSortKey::EndTime: return QStringLiteral("endTime");
        case PointRecordSortKey::DeviceName: return QStringLiteral("deviceName");
        case PointRecordSortKey::IsLaser: return QStringLiteral("isLaser");
        case PointRecordSortKey::IsReadCode: return QStringLiteral("isReadCode");
        case PointRecordSortKey::DetailJsonPath:
        default: return QStringLiteral("detailJsonPath");
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
        preview.append("...");
    }
    return preview.join("、");
}

QFrame* createCompactMetricCard(QLabel*& titleLabel, QLabel*& valueLabel, const QString& title, QWidget* parent) {
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

PointRecordPage::PointRecordPage(LaserSpc::App::AppServiceFacade* facade, QWidget* parent)
    : BasePage(facade, parent) {
    setupUi();
    m_exportProgressDialog = new ExportProgressDialog(this);
    m_queryWatcher = new QFutureWatcher<PointRecordQueryTaskResult>(this);
    m_exportWatcher = new QFutureWatcher<PointRecordExportTaskResult>(this);
    connect(m_queryWatcher, &QFutureWatcher<PointRecordQueryTaskResult>::finished, this, &PointRecordPage::handleQueryFinished);
    connect(m_exportWatcher, &QFutureWatcher<PointRecordExportTaskResult>::finished, this, &PointRecordPage::handleExportFinished);
}

LaserSpc::Domain::PageId PointRecordPage::pageId() const {
    return LaserSpc::Domain::PageId::PointRecord;
}

QString PointRecordPage::pageTitle() const {
    return textFor(uiEnglish(), "点位记录", "Point Records");
}

void PointRecordPage::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    const auto container = buildScrollablePageContainer(this, rootLayout);
    auto* layout = container.contentLayout;
    m_exportContentWidget = container.contentWidget;

    m_hintLabel = new QLabel(tr("点位记录支持分页查询，可按镭射内容或读码内容筛选，并可点击点位查看对应 JSON 详情。"), m_exportContentWidget);
    m_hintLabel->setProperty("hint", QVariant(true));
    m_hintLabel->setWordWrap(true);

    auto* metricLayout = new QHBoxLayout();
    metricLayout->setSpacing(10);
    metricLayout->addWidget(createCompactMetricCard(m_readCoverageTitleLabel, m_readCoverageValueLabel, tr("读码覆盖率"), m_exportContentWidget));
    metricLayout->addWidget(createCompactMetricCard(m_laserCoverageTitleLabel, m_laserCoverageValueLabel, tr("镭射覆盖率"), m_exportContentWidget));
    metricLayout->addWidget(createCompactMetricCard(m_avgDurationTitleLabel, m_avgDurationValueLabel, tr("平均处理时长"), m_exportContentWidget));

    auto* chartSplitter = new QSplitter(Qt::Horizontal, m_exportContentWidget);
    chartSplitter->setChildrenCollapsible(false);

    m_gradeBox = new QGroupBox(tr("等级分布"), m_exportContentWidget);
    UiTheme::applyPanel(m_gradeBox);
    auto* gradeLayout = new QVBoxLayout(m_gradeBox);
    m_gradeChartView = new ChartViewType(m_gradeBox);
    prepareChartView(m_gradeChartView);
    gradeLayout->addWidget(m_gradeChartView);

    m_deviceBox = new QGroupBox(tr("设备负载"), m_exportContentWidget);
    UiTheme::applyPanel(m_deviceBox);
    auto* deviceLayout = new QVBoxLayout(m_deviceBox);
    m_deviceChartView = new ChartViewType(m_deviceBox);
    prepareChartView(m_deviceChartView);
    deviceLayout->addWidget(m_deviceChartView);

    chartSplitter->addWidget(m_gradeBox);
    chartSplitter->addWidget(m_deviceBox);
    chartSplitter->setStretchFactor(0, 1);
    chartSplitter->setStretchFactor(1, 1);

    m_resultToolbar = new ResultToolbar(tr("导出 CSV"), tr("导出截图"), m_exportContentWidget);
    m_table = new QTableWidget(m_exportContentWidget);
    m_table->setMinimumHeight(560);
    configureDataTable(m_table,
                       {tr("程序名"),
                        tr("板号"),
                        tr("点位"),
                        tr("结果"),
                        tr("读码等级"),
                        tr("镭射内容"),
                        tr("读码内容"),
                        tr("开始时间"),
                        tr("结束时间"),
                        tr("设备"),
                        tr("是否镭射"),
                        tr("是否读码"),
                        tr("详情路径")},
                       0);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setMouseTracking(true);
    m_table->viewport()->setMouseTracking(true);

    layout->addWidget(m_hintLabel);
    layout->addLayout(metricLayout);
    chartSplitter->setSizes({460, 380});
    layout->addWidget(m_resultToolbar);
    layout->addWidget(m_table, 11);
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
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        showPointDetail(row);
    });
    connect(m_table, &QTableWidget::cellEntered, this, [this](int row, int column) {
        if (row < 0 || row >= m_currentResult.rows.size()) {
            return;
        }
        auto* item = m_table->item(row, column);
        if (item == nullptr || item->toolTip().trimmed().isEmpty()) {
            return;
        }
        QToolTip::showText(QCursor::pos(), item->toolTip(), m_table);
    });
    connect(m_resultToolbar, &ResultToolbar::exportCsvRequested, this, &PointRecordPage::exportCurrentQueryToCsv);
    connect(m_resultToolbar, &ResultToolbar::exportImageRequested, this, &PointRecordPage::exportCurrentPageScreenshot);
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
                                 QStringLiteral("point_record_report"),
                                 QStringLiteral("point_records"),
                                 criteriaSummary,
                                 metricSummary,
                                 this);
        dialog.exec();
    });
}

void PointRecordPage::reload(const LaserSpc::Domain::FilterCriteria& criteria) {
    m_currentCriteria = criteria;
    queryAndRender(true);
}

QList<PageInsightMetric> PointRecordPage::pageInsights(bool english) const {
    QMap<QString, int> gradeCounts;
    QMap<QString, int> deviceCounts;
    int ngCount = 0;
    for (const auto& row : m_currentResult.rows) {
        gradeCounts[row.readGrade] += 1;
        deviceCounts[row.deviceName] += 1;
        if (row.result == QObject::tr("NG")) {
            ++ngCount;
        }
    }

    const QString pending = english ? QObject::tr("Waiting") : tr("待加载");
    auto dominantGrade = gradeCounts.cend();
    for (auto it = gradeCounts.cbegin(); it != gradeCounts.cend(); ++it) {
        if (dominantGrade == gradeCounts.cend() || it.value() > dominantGrade.value()) {
            dominantGrade = it;
        }
    }
    auto hottestDevice = deviceCounts.cend();
    for (auto it = deviceCounts.cbegin(); it != deviceCounts.cend(); ++it) {
        if (hottestDevice == deviceCounts.cend() || it.value() > hottestDevice.value()) {
            hottestDevice = it;
        }
    }
    const qreal ngRate = m_currentResult.total == 0 ? 0.0 : static_cast<qreal>(ngCount) * 100.0 / static_cast<qreal>(m_currentResult.total);
    return {
        {english ? QObject::tr("Record volume") : tr("记录总量"),
         m_currentResult.total == 0 ? pending
                                    : (english ? QObject::tr("%1 rows").arg(m_currentResult.total) : tr("%1 条").arg(m_currentResult.total))},
        {english ? QObject::tr("NG ratio") : tr("NG占比"),
         m_currentResult.total == 0 ? pending : QObject::tr("%1% | %2").arg(QString::number(ngRate, 'f', 1)).arg(ngCount)},
        {english ? QObject::tr("Main grade") : tr("主等级"),
         dominantGrade == gradeCounts.cend() ? pending
                                             : QObject::tr("%1 | %2").arg(dominantGrade.key()).arg(dominantGrade.value())},
        {english ? QObject::tr("Hot device") : tr("热点设备"),
         hottestDevice == deviceCounts.cend() ? pending
                                              : QObject::tr("%1 | %2").arg(hottestDevice.key()).arg(hottestDevice.value())},
        {english ? QObject::tr("Read code hit") : tr("读码覆盖"),
         m_currentResult.rows.isEmpty()
             ? pending
             : (english ? QObject::tr("%1 non-empty").arg(std::count_if(m_currentResult.rows.cbegin(),
                                                                           m_currentResult.rows.cend(),
                                                                           [](const auto& row) { return row.isReadCode; }))
                        : tr("%1 条有读码").arg(std::count_if(m_currentResult.rows.cbegin(),
                                                              m_currentResult.rows.cend(),
                                                              [](const auto& row) { return row.isReadCode; })))}
    };
}

LaserSpc::Domain::PointRecordQuery PointRecordPage::buildQueryForCurrentView() const {
    LaserSpc::Domain::PointRecordQuery query;
    query.filter = m_currentCriteria;
    query.pagination.page = m_currentResult.page <= 0 ? 1 : m_currentResult.page;
    query.pagination.pageSize = qMax(10, m_resultToolbar->pageSize());
    query.sort = m_currentSort;
    return query;
}

void PointRecordPage::queryAndRender(bool resetPage) {
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

void PointRecordPage::startQuery(quint64 requestId) {
    const auto query = buildQueryForCurrentView();
    applyPageState(PageLoadState::Loading, isExportBusy());
    m_hintLabel->setText(tr("正在加载点位记录..."));
    emit statusMessageChanged(StatusText::pageQueryLoading(kStatusSubject));
    runPageTask(m_queryWatcher, [facade = m_facade, query, requestId]() {
        PointRecordQueryTaskResult result;
        result.requestId = requestId;
        const auto service = facade->recordQueryService();
        result.data = service.queryPointRecords(query);
        result.repositoryError = service.lastRepositoryError();
        return result;
    });
}

void PointRecordPage::handleQueryFinished() {
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
                         item.pointName,
                         item.result,
                         item.readGrade,
                         item.laserContent,
                         item.readCodeContent,
                         item.startTime.toString("yyyy-MM-dd HH:mm:ss"),
                         item.endTime.toString("yyyy-MM-dd HH:mm:ss"),
                         item.deviceName,
                         boolText(uiEnglish(), item.isLaser),
                         boolText(uiEnglish(), item.isReadCode),
                         item.detailJsonPath});
        const QString detailHint = item.detailJsonPath.trimmed().isEmpty()
                                       ? tr("当前点位未配置详情文件")
                                       : tr("双击任意单元格查看点位详情\n详情路径：%1").arg(item.detailJsonPath);
        for (int column = 0; column < m_table->columnCount(); ++column) {
            if (auto* tableItem = m_table->item(row, column)) {
                tableItem->setToolTip(detailHint);
            }
        }
    }
    m_table->setUpdatesEnabled(true);
    int readCount = 0;
    int laserCount = 0;
    qint64 totalDurationMs = 0;
    int durationCount = 0;
    for (const auto& item : m_currentResult.rows) {
        if (item.isReadCode) {
            ++readCount;
        }
        if (item.isLaser) {
            ++laserCount;
        }
        if (item.startTime.isValid() && item.endTime.isValid() && item.endTime >= item.startTime) {
            totalDurationMs += item.startTime.msecsTo(item.endTime);
            ++durationCount;
        }
    }
    const int rowCount = m_currentResult.rows.size();
    if (m_readCoverageValueLabel != nullptr) {
        m_readCoverageValueLabel->setText(rowCount == 0 ? tr("--")
                                                        : tr("%1%").arg(QString::number(static_cast<qreal>(readCount) * 100.0 / rowCount, 'f', 1)));
    }
    if (m_laserCoverageValueLabel != nullptr) {
        m_laserCoverageValueLabel->setText(rowCount == 0 ? tr("--")
                                                         : tr("%1%").arg(QString::number(static_cast<qreal>(laserCount) * 100.0 / rowCount, 'f', 1)));
    }
    if (m_avgDurationValueLabel != nullptr) {
        m_avgDurationValueLabel->setText(durationCount == 0 ? tr("--")
                                                            : tr("%1 ms").arg(QString::number(totalDurationMs / durationCount)));
    }
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
        m_hintLabel->setText(tr("当前筛选条件下暂无点位记录数据。"));
        emit statusMessageChanged(StatusText::pageNoData(kStatusSubject));
    } else {
        m_hintLabel->setText(tr("已加载 %1 条点位记录，当前排序 %2，页码 %3/%4。")
                                 .arg(m_currentResult.total)
                                 .arg(m_currentSort.field)
                                 .arg(m_currentResult.page)
                                 .arg(totalPages));
        emit statusMessageChanged(StatusText::pagedRefresh(kStatusSubject, m_currentResult.page, m_currentResult.total, m_currentSort.field));
    }
    updatePaginationUi();
    applyPageState(nextLoadState, isExportBusy());
}

void PointRecordPage::renderCharts() {
    QMap<QString, int> gradeCounts;
    QMap<QString, int> deviceCounts;
    for (const auto& row : m_currentResult.rows) {
        gradeCounts[row.readGrade] += 1;
        deviceCounts[row.deviceName] += 1;
    }

    auto* gradeChart = new ChartType();
    styleChart(gradeChart, tr("等级分布"));
    auto* gradeSeries = new PieSeriesType(gradeChart);
    gradeSeries->setHoleSize(0.45);
    const QList<QColor> pieColors{chartPalette().teal, chartPalette().blue, chartPalette().amber,
                                  chartPalette().coral, chartPalette().slate};
    int colorIndex = 0;
    for (auto it = gradeCounts.cbegin(); it != gradeCounts.cend(); ++it, ++colorIndex) {
        auto* slice = gradeSeries->append(it.key(), it.value());
        const QColor sliceColor = pieColors.at(colorIndex % pieColors.size());
        slice->setColor(sliceColor);
        slice->setLabel(QObject::tr("%1 %2").arg(it.key()).arg(it.value()));
        slice->setLabelVisible(false);
        attachPieSliceHoverBehavior(gradeChart,
                                    gradeSeries,
                                    slice,
                                    tr("%1\n数量：%2").arg(it.key()).arg(it.value()),
                                    sliceColor);
        connect(slice, &PieSliceType::clicked, this, [this, key = it.key()]() { selectRowsByTexts(4, {key}); });
    }
    gradeChart->addSeries(gradeSeries);
    refreshPieLegend(gradeChart, gradeSeries);
    m_gradeChartView->setChart(gradeChart);

    const QList<NamedCountBucket> deviceBuckets = aggregateNamedCounts(deviceCounts, uiEnglish());
    auto* deviceChart = new ChartType();
    styleChart(deviceChart, tr("设备负载"));
    auto* deviceSeries = new BarSeriesType(deviceChart);
    auto* deviceSet = new BarSetType(tr("记录数"));
    deviceSet->setColor(chartPalette().teal);
    QStringList categories;
    int maxValue = 0;
    for (const auto& bucket : deviceBuckets) {
        categories << bucket.label;
        *deviceSet << bucket.count;
        maxValue = qMax(maxValue, bucket.count);
    }
    deviceSeries->append(deviceSet);
    deviceChart->addSeries(deviceSeries);

    auto* axisX = new BarCategoryAxisType(deviceChart);
    axisX->append(categories);
    styleCategoryAxis(axisX);
    axisX->setLabelsVisible(false);
    deviceChart->addAxis(axisX, Qt::AlignBottom);
    deviceSeries->attachAxis(axisX);

    auto* axisY = new ValueAxisType(deviceChart);
    axisY->setRange(0, qMax(1, maxValue + 1));
    axisY->setLabelFormat(QObject::tr("%d"));
    styleValueAxis(axisY, tr("记录数"));
    deviceChart->addAxis(axisY, Qt::AlignLeft);
    deviceSeries->attachAxis(axisY);

    connect(deviceSet, &BarSetType::hovered, this, [this, deviceSet, deviceBuckets](bool status, int index) {
        deviceSet->setColor(status ? chartPalette().tealHover : chartPalette().teal);
        if (!status || index < 0 || index >= deviceBuckets.size()) {
            return;
        }
        const auto& bucket = deviceBuckets.at(index);
        QToolTip::showText(QCursor::pos(),
                           QObject::tr("设备：%1\n记录数：%2").arg(bucket.label).arg(bucket.count),
                           m_deviceChartView);
    });
    connect(deviceSet, &BarSetType::clicked, this, [this, deviceBuckets](int index) {
        if (index >= 0 && index < deviceBuckets.size() && !deviceBuckets.at(index).sourceLabels.isEmpty()) {
            selectRowsByTexts(9, deviceBuckets.at(index).sourceLabels);
        }
    });
    m_deviceChartView->setChart(deviceChart);
}

void PointRecordPage::selectRowsByTexts(int column, const QStringList& values) {
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
        emit statusMessageChanged(tr("已选中 %1 条点位记录，命中 %2 个图表类目。").arg(selectedCount).arg(values.size()));
    }
}

void PointRecordPage::showPointDetail(int row) {
    if (row < 0 || row >= m_currentResult.rows.size()) {
        return;
    }

    const auto& item = m_currentResult.rows.at(row);
    if (item.detailJsonPath.trimmed().isEmpty()) {
        emit statusMessageChanged(tr("点位 %1 未配置详情 JSON 路径。").arg(item.pointName));
        return;
    }

    QJsonObject detailObject;
    LaserSpc::Domain::PointDetailInfo detail;
    QString errorMessage;
    const QString resolvedPath = Infrastructure::PointDetailJsonService::resolveDetailFilePath(item.detailJsonPath);
    if (!Infrastructure::PointDetailJsonService::loadDetail(item.detailJsonPath, &detail, &errorMessage, &detailObject)) {
        emit statusMessageChanged(tr("读取点位详情失败：%1").arg(errorMessage));
        return;
    }

    PointDetailDialog dialog(this);
    dialog.setDetailFilePath(resolvedPath);
    dialog.setDetailObject(detailObject);
    dialog.exec();
}

void PointRecordPage::handleExportFinished() {
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

void PointRecordPage::applyPageState(PageLoadState loadState, bool exportBusy) {
    setLoadState(loadState);
    setExportBusy(exportBusy);
    m_resultToolbar->setEnabled(!isInteractionLocked());
    m_resultToolbar->setExportControlsEnabled(m_facade->settings().exportReportEnabled && !isInteractionLocked());
    m_table->setEnabled(!isQueryBusy());
}

void PointRecordPage::updatePaginationUi() {
    m_resultToolbar->setCurrentPage(m_currentResult.page);
    m_resultToolbar->setTotalRows(m_currentResult.total);
}

void PointRecordPage::exportCurrentQueryToCsv() {
    if (isExportBusy()) {
        emit statusMessageChanged(StatusText::exportBusy(kStatusSubject));
        return;
    }

    LaserSpc::Domain::PointRecordQuery query;
    query.filter = m_currentCriteria;
    query.pagination.page = 1;
    query.pagination.pageSize = qMax(1, m_currentResult.total);
    query.sort = m_currentSort;

    applyPageState(loadState(), true);
    m_resultToolbar->setTaskState(tr("正在导出点位记录 CSV..."), false);
    m_exportProgressDialog->startTask(tr("导出点位记录"), tr("正在生成 CSV 文件"));
    emit statusMessageChanged(StatusText::exportStarted(kExportSubject, QObject::tr(" CSV ")));
    runPageTask(m_exportWatcher, [facade = m_facade, query]() {
        PointRecordExportTaskResult result;
        const auto rows = facade->recordQueryService().queryPointRecords(query).rows;
        result.success = Infrastructure::ExportService::exportPointRecordsToCsv(rows, &result.outputPath, &result.errorMessage);
        return result;
    });
}

void PointRecordPage::exportCurrentPageScreenshot() {
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
    m_resultToolbar->setTaskState(tr("正在导出点位记录截图..."), false);
    m_exportProgressDialog->startTask(tr("导出点位记录"), tr("正在生成截图文件"));
    emit statusMessageChanged(StatusText::exportStarted(kExportSubject, tr("截图")));
    runPageTask(m_exportWatcher, [pixmap]() {
        PointRecordExportTaskResult result;
        result.success = Infrastructure::ExportService::exportPixmapScreenshot(
            pixmap, QStringLiteral("point_records_screenshot"), &result.outputPath, &result.errorMessage);
        return result;
    });
}

void PointRecordPage::refreshTexts(bool english) {
    m_hintLabel->setText(textFor(english,
                                 "点位记录支持分页查询，可按镭射内容或读码内容筛选，并可点击点位查看对应 JSON 详情。",
                                 "Point records support paged queries, filtering by laser or read-code content, and opening JSON details from rows."));
    if (m_readCoverageTitleLabel != nullptr) m_readCoverageTitleLabel->setText(textFor(english, "读码覆盖率", "Read Coverage"));
    if (m_laserCoverageTitleLabel != nullptr) m_laserCoverageTitleLabel->setText(textFor(english, "镭射覆盖率", "Laser Coverage"));
    if (m_avgDurationTitleLabel != nullptr) m_avgDurationTitleLabel->setText(textFor(english, "平均处理时长", "Avg Duration"));
    if (m_gradeBox != nullptr) m_gradeBox->setTitle(textFor(english, "等级分布", "Grade Distribution"));
    if (m_deviceBox != nullptr) m_deviceBox->setTitle(textFor(english, "设备负载", "Device Load"));
    if (m_resultToolbar != nullptr) {
        m_resultToolbar->setTexts(english,
                                  textFor(english, "导出 CSV", "Export CSV"),
                                  textFor(english, "导出截图", "Export Screenshot"));
    }
    configureDataTable(m_table,
                       {textFor(english, "程序名", "Program"),
                        textFor(english, "板号", "Board Code"),
                        textFor(english, "点位", "Point"),
                        textFor(english, "结果", "Result"),
                        textFor(english, "读码等级", "Read Grade"),
                        textFor(english, "镭射内容", "Laser Content"),
                        textFor(english, "读码内容", "Read Content"),
                        textFor(english, "开始时间", "Start Time"),
                        textFor(english, "结束时间", "End Time"),
                        textFor(english, "设备", "Device"),
                        textFor(english, "是否镭射", "Laser"),
                        textFor(english, "是否读码", "Read Code"),
                        textFor(english, "详情路径", "Detail Path")},
                       0);
    renderCharts();
}

}  // namespace LaserSpc::Ui
