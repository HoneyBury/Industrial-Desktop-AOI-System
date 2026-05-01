#include "ui/pages/BadStatPage.h"
#include <QVariant>

#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QCursor>
#include <QScrollArea>
#include <QSplitter>
#include <QToolTip>
#include <QVBoxLayout>
#include <QtMath>

#include "infrastructure/ExportService.h"
#include "ui/common/StatusText.h"
#include "ui/common/UiTheme.h"
#include "ui/dialogs/ExportPanelDialog.h"
#include "ui/dialogs/ExportProgressDialog.h"
#include "ui/pages/ChartVisuals.h"
#include "ui/pages/PageContainer.h"
#include "ui/pages/TableRenderSupport.h"
#include "ui/widgets/ResultToolbar.h"

namespace LaserSpc::Ui {

namespace {

QString textFor(bool english, const char* chinese, const char* englishText) {
    return english ? QObject::tr(englishText) : QObject::tr(chinese);
}

const QString kStatusSubject = QObject::tr("不良统计页");
const QString kExportSubject = QObject::tr("不良统计");

QFrame* createMetricCard(QLabel*& valueLabel, QLabel*& descLabel, const QString& title, QWidget* parent) {
    auto* box = new QFrame(parent);
    UiTheme::applyPanel(box);
    box->setProperty("metricCard", QVariant(true));
    box->setMinimumWidth(0);
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(6);

    auto* titleLabel = new QLabel(title, box);
    titleLabel->setProperty("metricTitle", QVariant(true));
    valueLabel = new QLabel(QObject::tr("--"), box);
    valueLabel->setProperty("metricValue", QVariant(true));
    descLabel = new QLabel(box);
    descLabel->setProperty("metricDesc", QVariant(true));
    descLabel->setWordWrap(true);

    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    layout->addWidget(descLabel);
    return box;
}

QList<LaserSpc::Domain::BadPointStatRow> compactBadPointChartRows(const QList<LaserSpc::Domain::BadPointStatRow>& rows, bool english) {
    if (rows.size() <= 6) {
        return rows;
    }

    QList<LaserSpc::Domain::BadPointStatRow> compacted;
    compacted.reserve(6);
    for (int index = 0; index < 5 && index < rows.size(); ++index) {
        compacted.append(rows.at(index));
    }

    LaserSpc::Domain::BadPointStatRow others;
    others.badPointName = english ? QObject::tr("Others") : QObject::tr("其他");
    for (int index = 5; index < rows.size(); ++index) {
        others.count += rows.at(index).count;
        others.ratio += rows.at(index).ratio;
    }
    compacted.append(others);
    return compacted;
}

QString shortBadPointAxisLabel(const QString& name, int index) {
    return QString::number(index + 1);
}

}  // namespace

BadStatPage::BadStatPage(LaserSpc::App::AppServiceFacade* facade, QWidget* parent)
    : BasePage(facade, parent) {
    setupUi();
    m_exportProgressDialog = new ExportProgressDialog(this);
    m_queryWatcher = new QFutureWatcher<BadStatQueryTaskResult>(this);
    m_exportWatcher = new QFutureWatcher<BadStatExportTaskResult>(this);
    connect(m_queryWatcher, &QFutureWatcher<BadStatQueryTaskResult>::finished, this, &BadStatPage::handleQueryFinished);
    connect(m_exportWatcher, &QFutureWatcher<BadStatExportTaskResult>::finished, this, &BadStatPage::handleExportFinished);
}

LaserSpc::Domain::PageId BadStatPage::pageId() const {
    return LaserSpc::Domain::PageId::BadStat;
}

QString BadStatPage::pageTitle() const {
    return textFor(uiEnglish(), "不良统计", "Bad Statistics");
}

void BadStatPage::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    const auto container = buildScrollablePageContainer(this, rootLayout);
    auto* contentLayout = container.contentLayout;
    m_exportContentWidget = container.contentWidget;

    m_hintLabel = new QLabel(QObject::tr("不良统计页已改成紧凑双列布局，Top 不良点和等级分布并排展示，信息密度更接近其他页面。"), this);
    m_hintLabel->setProperty("hint", QVariant(true));
    m_hintLabel->setWordWrap(true);

    auto* metricLayout = new QGridLayout();
    metricLayout->setHorizontalSpacing(10);
    metricLayout->setVerticalSpacing(10);
    metricLayout->addWidget(createMetricCard(m_totalBadPointsValue, m_totalBadPointsDesc, QObject::tr("不良点总数"), m_exportContentWidget), 0, 0);
    metricLayout->addWidget(createMetricCard(m_topBadPointValue, m_topBadPointDesc, QObject::tr("最高占比"), m_exportContentWidget), 0, 1);
    metricLayout->addWidget(createMetricCard(m_gradeCoverageValue, m_gradeCoverageDesc, QObject::tr("等级覆盖"), m_exportContentWidget), 0, 2);
    metricLayout->setColumnStretch(0, 1);
    metricLayout->setColumnStretch(1, 1);
    metricLayout->setColumnStretch(2, 1);

    m_resultToolbar = new ResultToolbar(QObject::tr("导出统计 CSV"), QObject::tr("导出统计截图"), this);

    auto* statSplitter = new QSplitter(Qt::Horizontal, m_exportContentWidget);
    statSplitter->setChildrenCollapsible(false);

    m_topBox = new QGroupBox(QObject::tr("Top 不良点"), m_exportContentWidget);
    UiTheme::applyPanel(m_topBox);
    auto* topLayout = new QVBoxLayout(m_topBox);
    topLayout->setContentsMargins(12, 14, 12, 12);
    topLayout->setSpacing(10);
    m_badPointChartsSplitter = new QSplitter(Qt::Vertical, m_topBox);
    m_badPointChartsSplitter->setChildrenCollapsible(false);
    m_badPointChartView = new ChartViewType(m_topBox);
    prepareChartView(m_badPointChartView);
    m_badPointShareChartView = new ChartViewType(m_topBox);
    prepareChartView(m_badPointShareChartView);
    m_badPointShareChartView->setMinimumWidth(260);
    m_badPointShareChartView->setMinimumHeight(300);
    m_badPointTable = new QTableWidget(this);
    configureDataTable(m_badPointTable, {QObject::tr("不良点"), QObject::tr("次数"), QObject::tr("占比")});
    m_badPointTable->setMinimumHeight(220);
    m_badPointChartsSplitter->addWidget(m_badPointChartView);
    m_badPointChartsSplitter->addWidget(m_badPointShareChartView);
    topLayout->addWidget(m_badPointChartsSplitter, 3);
    topLayout->addWidget(m_badPointTable, 2);

    m_gradeBox = new QGroupBox(QObject::tr("读码等级分布"), m_exportContentWidget);
    UiTheme::applyPanel(m_gradeBox);
    auto* gradeLayout = new QVBoxLayout(m_gradeBox);
    gradeLayout->setContentsMargins(12, 14, 12, 12);
    gradeLayout->setSpacing(10);
    m_gradeChartView = new ChartViewType(m_gradeBox);
    prepareChartView(m_gradeChartView);
    m_gradeTable = new QTableWidget(this);
    configureDataTable(m_gradeTable, {QObject::tr("等级"), QObject::tr("数量"), QObject::tr("占比")});
    m_gradeTable->setMinimumHeight(220);
    gradeLayout->addWidget(m_gradeChartView, 3);
    gradeLayout->addWidget(m_gradeTable, 2);

    statSplitter->addWidget(m_gradeBox);
    statSplitter->addWidget(m_topBox);
    statSplitter->setStretchFactor(0, 1);
    statSplitter->setStretchFactor(1, 1);
    statSplitter->setSizes({700, 700});
    updateBadPointChartsLayout();

    contentLayout->addWidget(m_hintLabel);
    contentLayout->addLayout(metricLayout);
    contentLayout->addWidget(m_resultToolbar);
    contentLayout->addWidget(statSplitter);

    connect(m_resultToolbar, &ResultToolbar::exportCsvRequested, this, &BadStatPage::exportCurrentStatsToCsv);
    connect(m_resultToolbar, &ResultToolbar::exportImageRequested, this, &BadStatPage::exportCurrentPageScreenshot);
    connect(m_resultToolbar, &ResultToolbar::exportPanelRequested, this, [this]() {
        QStringList criteriaSummary{
            QObject::tr("时间范围：%1 至 %2")
                .arg(m_currentCriteria.beginTime.toString("yyyy-MM-dd HH:mm:ss"),
                     m_currentCriteria.endTime.toString("yyyy-MM-dd HH:mm:ss")),
            QObject::tr("线体：%1").arg(m_currentCriteria.lineName),
            QObject::tr("程序：%1").arg(m_currentCriteria.programName),
            QObject::tr("设备：%1").arg(m_currentCriteria.deviceName),
            QObject::tr("结果：%1").arg(m_currentCriteria.result)
        };
        QStringList metricSummary{
            QObject::tr("Top 不良点：%1 项").arg(m_currentBadPoints.size()),
            QObject::tr("等级分布：%1 项").arg(m_currentGrades.size())
        };
        ExportPanelDialog dialog(m_resultToolbar->taskState(),
                                 QStringLiteral("bad_stat_report"),
                                 QStringLiteral("bad_stats"),
                                 criteriaSummary,
                                 metricSummary,
                                 this);
        dialog.exec();
    });
    connect(m_badPointTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row >= 0 && row < m_currentBadPoints.size()) {
            drillDownToPointRecordsByBadPoint(m_currentBadPoints.at(row).badPointName);
        }
    });
    connect(m_gradeTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row >= 0 && row < m_currentGrades.size()) {
            drillDownToPointRecordsByGrade(m_currentGrades.at(row).grade);
        }
    });
}

void BadStatPage::resizeEvent(QResizeEvent* event) {
    BasePage::resizeEvent(event);
    updateBadPointChartsLayout();
}

void BadStatPage::updateBadPointChartsLayout() {
    if (m_badPointChartsSplitter == nullptr || m_topBox == nullptr) {
        return;
    }

    const int availableWidth = m_topBox->contentsRect().width();
    const bool useHorizontalLayout = availableWidth >= 860;
    const Qt::Orientation targetOrientation = useHorizontalLayout ? Qt::Horizontal : Qt::Vertical;
    if (m_badPointChartsSplitter->orientation() != targetOrientation) {
        m_badPointChartsSplitter->setOrientation(targetOrientation);
    }

    if (useHorizontalLayout) {
        m_badPointChartsSplitter->setSizes({availableWidth * 3 / 5, availableWidth * 2 / 5});
    } else {
        const int availableHeight = qMax(640, m_topBox->height() - m_badPointTable->minimumHeight() - 40);
        m_badPointChartsSplitter->setSizes({availableHeight / 2, availableHeight / 2});
    }
}

void BadStatPage::reload(const LaserSpc::Domain::FilterCriteria& criteria) {
    m_currentCriteria = criteria;
    const quint64 requestId = m_querySequence.registerRequest(isQueryBusy());
    if (requestId == 0) {
        emit statusMessageChanged(StatusText::pagePendingRefresh(kStatusSubject));
        return;
    }

    startQuery(requestId);
}

QList<PageInsightMetric> BadStatPage::pageInsights(bool english) const {
    const QString pending = english ? QObject::tr("Waiting") : QObject::tr("待加载");
    const auto topPoint = m_currentBadPoints.isEmpty() ? LaserSpc::Domain::BadPointStatRow{} : m_currentBadPoints.first();
    const auto topGrade = m_currentGrades.isEmpty() ? LaserSpc::Domain::GradeStatRow{} : m_currentGrades.first();
    return {
        {english ? QObject::tr("Point count") : QObject::tr("缺陷点数"),
         m_totalBadPoints <= 0 ? pending : QObject::tr("%1 点").arg(m_totalBadPoints)},
        {english ? QObject::tr("Top defect") : QObject::tr("最高不良"),
         m_currentBadPoints.isEmpty() ? pending
                                      : QObject::tr("%1 | %2%").arg(topPoint.badPointName).arg(QString::number(topPoint.ratio, 'f', 1))},
        {english ? QObject::tr("Grade risk") : QObject::tr("等级风险"),
         m_currentGrades.isEmpty() ? pending
                                   : QObject::tr("%1 | %2%").arg(topGrade.grade).arg(QString::number(topGrade.ratio, 'f', 1))},
        {english ? QObject::tr("Coverage") : QObject::tr("统计覆盖"),
         (m_currentBadPoints.isEmpty() && m_currentGrades.isEmpty()) ? pending
                                                                     : QObject::tr("%1 点 / %2 级")
                                                                           .arg(m_currentBadPoints.size())
                                                                           .arg(m_currentGrades.size())}
    };
}

void BadStatPage::startQuery(quint64 requestId) {
    LaserSpc::Domain::BadStatQuery query;
    query.filter = m_currentCriteria;
    query.topN = 10;
    applyPageState(PageLoadState::Loading, isExportBusy());
    m_hintLabel->setText(QObject::tr("不良统计查询中，请稍候..."));
    emit statusMessageChanged(StatusText::pageQueryLoading(kStatusSubject));
    runPageTask(m_queryWatcher, [facade = m_facade, query, requestId]() {
        BadStatQueryTaskResult result;
        result.requestId = requestId;
        const auto service = facade->statQueryService();
        result.data = service.queryBadStatistics(query);
        result.repositoryError = service.lastRepositoryError();
        return result;
    });
}

void BadStatPage::handleQueryFinished() {
    const auto result = m_queryWatcher->result();
    if (!acceptLatestPageResult(result, m_querySequence, result.requestId, [this](quint64 nextRequestId) {
            startQuery(nextRequestId);
        })) {
        return;
    }

    const auto& data = result.data;
    m_currentBadPoints = data.badPoints;
    m_currentGrades = data.grades;
    m_totalBadPoints = data.totalBadPoints;
    m_totalGradePoints = data.totalGradePoints;

    m_badPointTable->setUpdatesEnabled(false);
    m_badPointTable->setRowCount(0);
    m_badPointTable->setRowCount(data.badPoints.size());
    for (int row = 0; row < data.badPoints.size(); ++row) {
        const auto& item = data.badPoints.at(row);
        setTextTableRow(m_badPointTable,
                        row,
                        {item.badPointName,
                         QString::number(item.count),
                         QString::number(item.ratio, 'f', 2) + QObject::tr("%")});
    }
    m_badPointTable->setUpdatesEnabled(true);

    m_gradeTable->setUpdatesEnabled(false);
    m_gradeTable->setRowCount(0);
    m_gradeTable->setRowCount(data.grades.size());
    for (int row = 0; row < data.grades.size(); ++row) {
        const auto& item = data.grades.at(row);
        setTextTableRow(m_gradeTable,
                        row,
                        {item.grade,
                         QString::number(item.count),
                         QString::number(item.ratio, 'f', 2) + QObject::tr("%")});
    }
    m_gradeTable->setUpdatesEnabled(true);

    int totalBadPointCount = 0;
    for (const auto& item : data.badPoints) {
        totalBadPointCount += item.count;
    }
    const int allBadPointCount = data.totalBadPoints > 0 ? data.totalBadPoints : totalBadPointCount;
    m_totalBadPointsValue->setText(QString::number(allBadPointCount));
    m_totalBadPointsDesc->setText(QObject::tr("当前筛选日期内全部 NG 点位总数"));
    if (!data.badPoints.isEmpty()) {
        m_topBadPointValue->setText(QObject::tr("%1%").arg(QString::number(data.badPoints.first().ratio, 'f', 1)));
        m_topBadPointDesc->setText(QObject::tr("%1（%2 次）").arg(data.badPoints.first().badPointName).arg(data.badPoints.first().count));
    } else {
        m_topBadPointValue->setText(QObject::tr("--"));
        m_topBadPointDesc->setText(QObject::tr("暂无最高频不良点"));
    }
    int coveredGradeCount = 0;
    qreal coveredGradeRatio = 0.0;
    for (const auto& grade : data.grades) {
        if (grade.count > 0) {
            ++coveredGradeCount;
            coveredGradeRatio += grade.ratio;
        }
    }
    m_gradeCoverageValue->setText(QString::number(coveredGradeCount));
    m_gradeCoverageDesc->setText(
        QObject::tr("以筛选日期内全部点位为基准，覆盖 %1%")
            .arg(QString::number(qMin<qreal>(100.0, coveredGradeRatio), 'f', 1)));

    renderBadPointChart(data.badPoints);
    renderGradeChart(data.grades);

    PageLoadState nextLoadState = PageLoadState::Loaded;
    if (!result.repositoryError.isEmpty()) {
        nextLoadState = PageLoadState::Error;
        m_hintLabel->setText(QObject::tr("查询失败：") + result.repositoryError);
        emit statusMessageChanged(StatusText::pageQueryFailed(kStatusSubject, result.repositoryError));
    } else if (data.badPoints.isEmpty() && data.grades.isEmpty()) {
        nextLoadState = PageLoadState::Empty;
        m_hintLabel->setText(QObject::tr("当前筛选条件下暂无不良统计数据，请调整查询条件。"));
        emit statusMessageChanged(StatusText::pageNoData(kStatusSubject));
    } else {
        m_hintLabel->setText(QObject::tr("当前不良点 %1 项，等级分布 %2 项，左右模块联动展示。")
                                 .arg(data.badPoints.size())
                                 .arg(data.grades.size()));
        emit statusMessageChanged(StatusText::statRefresh(kStatusSubject, data.badPoints.size(), data.grades.size()));
    }
    applyPageState(nextLoadState, isExportBusy());
}

void BadStatPage::handleExportFinished() {
    const auto result = m_exportWatcher->result();
    if (!result.success) {
        m_resultToolbar->setTaskState(QObject::tr("导出任务：失败，") + result.errorMessage, true);
        m_exportProgressDialog->finishTask(false, tr("导出失败：%1").arg(result.errorMessage));
        emit statusMessageChanged(StatusText::exportFailed(kExportSubject, result.errorMessage));
    } else {
        m_resultToolbar->setTaskState(QObject::tr("导出任务：已完成，文件：") + result.outputPath, false);
        m_exportProgressDialog->finishTask(true, tr("导出完成：%1").arg(result.outputPath));
        emit statusMessageChanged(StatusText::exportCompleted(kExportSubject, result.outputPath));
    }
    applyPageState(loadState(), false);
}

void BadStatPage::applyPageState(PageLoadState loadState, bool exportBusy) {
    setLoadState(loadState);
    setExportBusy(exportBusy);
    m_resultToolbar->setEnabled(!isInteractionLocked());
    m_resultToolbar->setExportControlsEnabled(m_facade->settings().exportReportEnabled && !isInteractionLocked());
    m_badPointTable->setEnabled(!isQueryBusy());
    m_gradeTable->setEnabled(!isQueryBusy());
}

void BadStatPage::renderBadPointChart(const QList<LaserSpc::Domain::BadPointStatRow>& rows) {
    const QList<LaserSpc::Domain::BadPointStatRow> chartRows = compactBadPointChartRows(rows, uiEnglish());
    auto* chart = new ChartType();
    styleChart(chart, QObject::tr("Top 不良点分布"));

    auto* series = new BarSeriesType(chart);
    auto* set = new BarSetType(QObject::tr("不良次数"), series);
    set->setColor(chartPalette().teal);
    set->setBorderColor(chartPalette().teal.darker(120));

    QStringList categories;
    QStringList displayCategories;
    int maxValue = 0;
    for (int index = 0; index < chartRows.size(); ++index) {
        const auto& row = chartRows.at(index);
        *set << row.count;
        categories << row.badPointName;
        displayCategories << shortBadPointAxisLabel(row.badPointName, index);
        maxValue = qMax(maxValue, row.count);
    }

    series->append(set);
    chart->addSeries(series);

    auto* axisX = new BarCategoryAxisType(chart);
    axisX->append(displayCategories);
    styleCategoryAxis(axisX);
    axisX->setLabelsVisible(false);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new ValueAxisType(chart);
    axisY->setRange(0, qMax(1, maxValue + 2));
    axisY->setLabelFormat(QObject::tr("%d"));
    styleValueAxis(axisY, QObject::tr("次数"));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    connect(set, &BarSetType::clicked, this, [this, chartRows](int index) {
        if (index >= 0 && index < chartRows.size() && chartRows.at(index).badPointName == QObject::tr("其他")) {
            emit statusMessageChanged(QObject::tr("图表已将尾部不良点聚合为“其他”，请查看下方表格获取完整明细。"));
            return;
        }
        if (index >= 0 && index < chartRows.size()) {
            selectBadPointRow(chartRows.at(index).badPointName);
        }
    });
    connect(set, &BarSetType::hovered, this, [this, set, chartRows](bool status, int index) {
        set->setColor(status ? chartPalette().tealHover : chartPalette().teal);
        if (!status || index < 0 || index >= chartRows.size()) {
            return;
        }
        const auto& row = chartRows.at(index);
        QToolTip::showText(QCursor::pos(),
                           QObject::tr("不良点：%1\n次数：%2\n占比：%3%")
                               .arg(row.badPointName)
                               .arg(row.count)
                               .arg(QString::number(row.ratio, 'f', 2)),
                           m_badPointChartView);
    });

    m_badPointChartView->setChart(chart);

    auto* shareChart = new ChartType();
    styleChart(shareChart, QObject::tr("Top 占比结构"));
    shareChart->legend()->setVisible(false);
    auto* shareSeries = new PieSeriesType(shareChart);
    shareSeries->setHoleSize(0.52);
    const QList<QColor> colors{
        chartPalette().teal,
        chartPalette().blue,
        chartPalette().amber,
        chartPalette().coral,
        chartPalette().slate,
        chartPalette().axis
    };
    for (int index = 0; index < chartRows.size(); ++index) {
        const auto& row = chartRows.at(index);
        auto* slice = shareSeries->append(row.badPointName, row.count);
        const QColor sliceColor = colors.at(index % colors.size());
        slice->setColor(sliceColor);
        slice->setLabel(QObject::tr("%1%").arg(QString::number(row.ratio, 'f', 1)));
        slice->setLabelVisible(false);
        slice->setLabelArmLengthFactor(0.1);
        attachPieSliceHoverBehavior(shareChart,
                                    shareSeries,
                                    slice,
                                    QObject::tr("%1\n次数：%2\n占比：%3%")
                                        .arg(row.badPointName)
                                        .arg(row.count)
                                        .arg(QString::number(row.ratio, 'f', 2)),
                                    sliceColor);
        connect(slice, &PieSliceType::clicked, this, [this, row]() {
            if (row.badPointName == QObject::tr("其他")) {
                emit statusMessageChanged(QObject::tr("扇形图中的“其他”表示剩余不良点聚合项，请查看下方表格。"));
                return;
            }
            selectBadPointRow(row.badPointName);
        });
    }
    shareChart->addSeries(shareSeries);
    refreshPieLegend(shareChart, shareSeries);
    m_badPointShareChartView->setChart(shareChart);
}

void BadStatPage::renderGradeChart(const QList<LaserSpc::Domain::GradeStatRow>& rows) {
    auto* chart = new ChartType();
    styleChart(chart, QObject::tr("读码等级分布"));

    auto* series = new PieSeriesType(chart);
    series->setHoleSize(0.42);
    const QList<QColor> colors{chartPalette().teal, chartPalette().blue, chartPalette().amber,
                               chartPalette().coral, chartPalette().slate};
    int colorIndex = 0;
    for (const auto& row : rows) {
        auto* slice = series->append(row.grade, row.count);
        const QColor sliceColor = colors.at(colorIndex % colors.size());
        slice->setColor(sliceColor);
        ++colorIndex;
        slice->setLabel(QObject::tr("%1 %2%").arg(row.grade).arg(QString::number(row.ratio, 'f', 2)));
        slice->setLabelVisible(false);
        slice->setLabelArmLengthFactor(0.12);
        connect(slice, &PieSliceType::clicked, this, [this, row]() { selectGradeRow(row.grade); });
        attachPieSliceHoverBehavior(chart,
                                    series,
                                    slice,
                                    QObject::tr("%1\n数量：%2\n占比：%3%")
                                        .arg(row.grade)
                                        .arg(row.count)
                                        .arg(QString::number(row.ratio, 'f', 2)),
                                    sliceColor);
    }

    chart->addSeries(series);
    refreshPieLegend(chart, series);
    m_gradeChartView->setChart(chart);
}

void BadStatPage::selectBadPointRow(const QString& pointName) {
    for (int row = 0; row < m_badPointTable->rowCount(); ++row) {
        if (m_badPointTable->item(row, 0) != nullptr && m_badPointTable->item(row, 0)->text() == pointName) {
            m_badPointTable->selectRow(row);
            m_badPointTable->scrollToItem(m_badPointTable->item(row, 0));
            return;
        }
    }
}

void BadStatPage::selectGradeRow(const QString& grade) {
    for (int row = 0; row < m_gradeTable->rowCount(); ++row) {
        if (m_gradeTable->item(row, 0) != nullptr && m_gradeTable->item(row, 0)->text() == grade) {
            m_gradeTable->selectRow(row);
            m_gradeTable->scrollToItem(m_gradeTable->item(row, 0));
            return;
        }
    }
}

void BadStatPage::drillDownToPointRecordsByBadPoint(const QString& pointName) {
    auto criteria = m_currentCriteria;
    criteria.result = QObject::tr("NG");
    criteria.keyword = pointName;
    emit pointDrillDownRequested(criteria, QObject::tr("已从不良统计钻取到点位记录：不良点 %1。").arg(pointName));
}

void BadStatPage::drillDownToPointRecordsByGrade(const QString& grade) {
    auto criteria = m_currentCriteria;
    criteria.keyword = grade;
    emit pointDrillDownRequested(criteria, QObject::tr("已从不良统计钻取到点位记录：读码等级 %1。").arg(grade));
}

void BadStatPage::exportCurrentStatsToCsv() {
    if (isExportBusy()) {
        emit statusMessageChanged(StatusText::exportBusy(kStatusSubject));
        return;
    }

    const auto badPoints = m_currentBadPoints;
    const auto grades = m_currentGrades;
    applyPageState(loadState(), true);
    m_resultToolbar->setTaskState(QObject::tr("导出任务：正在生成不良统计 CSV..."), false);
    m_exportProgressDialog->startTask(tr("导出不良统计"), tr("正在生成 CSV 文件"));
    emit statusMessageChanged(StatusText::exportStarted(kExportSubject, QObject::tr(" CSV ")));
    runPageTask(m_exportWatcher, [badPoints, grades]() {
        BadStatExportTaskResult result;
        result.success = Infrastructure::ExportService::exportBadStatsToCsv(
            badPoints, grades, &result.outputPath, &result.errorMessage);
        return result;
    });
}

void BadStatPage::exportCurrentPageScreenshot() {
    if (isExportBusy()) {
        emit statusMessageChanged(StatusText::exportBusy(kStatusSubject));
        return;
    }

    const QPixmap pixmap = captureModuleSnapshot(m_exportContentWidget, {m_badPointTable, m_gradeTable});
    if (pixmap.isNull()) {
        emit statusMessageChanged(StatusText::exportFailed(kExportSubject + QObject::tr("截图"),
                                                           QObject::tr("无法抓取当前页面截图。")));
        return;
    }

    applyPageState(loadState(), true);
    m_resultToolbar->setTaskState(QObject::tr("导出任务：正在生成不良统计截图..."), false);
    m_exportProgressDialog->startTask(tr("导出不良统计"), tr("正在生成截图文件"));
    emit statusMessageChanged(StatusText::exportStarted(kExportSubject, QObject::tr("截图")));
    runPageTask(m_exportWatcher, [pixmap]() {
        BadStatExportTaskResult result;
        result.success = Infrastructure::ExportService::exportPixmapScreenshot(
            pixmap, QStringLiteral("bad_stats_screenshot"), &result.outputPath, &result.errorMessage);
        return result;
    });
}

void BadStatPage::refreshTexts(bool english) {
    m_hintLabel->setText(textFor(english,
                                 "不良统计页已改成紧凑双列布局，Top 不良点和等级分布并排展示，信息密度更接近其他页面。",
                                 "Bad statistics uses a compact two-column layout, showing top defects and grade distribution side by side."));
    if (m_topBox != nullptr) m_topBox->setTitle(textFor(english, "Top 不良点", "Top Defects"));
    if (m_gradeBox != nullptr) m_gradeBox->setTitle(textFor(english, "读码等级分布", "Read Grade Distribution"));
    if (m_resultToolbar != nullptr) {
        m_resultToolbar->setTexts(english,
                                  textFor(english, "导出统计 CSV", "Export Stats CSV"),
                                  textFor(english, "导出统计截图", "Export Stats Screenshot"));
    }
    configureDataTable(m_badPointTable,
                       {textFor(english, "不良点", "Defect Point"),
                        textFor(english, "次数", "Count"),
                        textFor(english, "占比", "Ratio")});
    configureDataTable(m_gradeTable,
                       {textFor(english, "等级", "Grade"),
                        textFor(english, "数量", "Count"),
                        textFor(english, "占比", "Ratio")});
    renderBadPointChart(m_currentBadPoints);
    renderGradeChart(m_currentGrades);
}

}  // namespace LaserSpc::Ui
