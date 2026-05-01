#pragma once

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChartView>
#include <QtCharts/QLegend>
#include <QtCharts/QLegendMarker>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieLegendMarker>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QValueAxis>

#include <QBrush>
#include <QColor>
#include <QCursor>
#include <QFont>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QRegularExpression>
#include <QScreen>
#include <QStringList>
#include <QToolTip>

namespace LaserSpc::Ui {

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using RawChartViewType = QChartView;
using ChartType = QChart;
using ChartAnimationOption = QChart::AnimationOption;
using ChartRubberBand = QChartView::RubberBand;
using BarSeriesType = QBarSeries;
using BarSetType = QBarSet;
using BarCategoryAxisType = QBarCategoryAxis;
using LineSeriesType = QLineSeries;
using ValueAxisType = QValueAxis;
using PieSeriesType = QPieSeries;
using PieSliceType = QPieSlice;
using PieLegendMarkerType = QPieLegendMarker;
#else
using RawChartViewType = QtCharts::QChartView;
using ChartType = QtCharts::QChart;
using ChartAnimationOption = QtCharts::QChart::AnimationOption;
using ChartRubberBand = QtCharts::QChartView::RubberBand;
using BarSeriesType = QtCharts::QBarSeries;
using BarSetType = QtCharts::QBarSet;
using BarCategoryAxisType = QtCharts::QBarCategoryAxis;
using LineSeriesType = QtCharts::QLineSeries;
using ValueAxisType = QtCharts::QValueAxis;
using PieSeriesType = QtCharts::QPieSeries;
using PieSliceType = QtCharts::QPieSlice;
using PieLegendMarkerType = QtCharts::QPieLegendMarker;
#endif

class ChartViewType : public RawChartViewType {
public:
    explicit ChartViewType(QWidget* parent = nullptr) : RawChartViewType(parent) {}

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event != nullptr && event->button() == Qt::RightButton) {
            if (chart() != nullptr) {
                chart()->zoomReset();
            }
            event->accept();
            return;
        }
        RawChartViewType::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event != nullptr && chart() != nullptr) {
            chart()->zoomReset();
            event->accept();
            return;
        }
        RawChartViewType::mouseDoubleClickEvent(event);
    }
};

struct ChartPalette {
    QColor teal = QColor(QObject::tr("#0b7285"));
    QColor tealHover = QColor(QObject::tr("#1098ad"));
    QColor blue = QColor(QObject::tr("#1971c2"));
    QColor blueHover = QColor(QObject::tr("#228be6"));
    QColor amber = QColor(QObject::tr("#f08c00"));
    QColor amberHover = QColor(QObject::tr("#f59f00"));
    QColor coral = QColor(QObject::tr("#d9480f"));
    QColor coralHover = QColor(QObject::tr("#e8590c"));
    QColor slate = QColor(QObject::tr("#5c677d"));
    QColor panel = QColor(QObject::tr("#ffffff"));
    QColor axis = QColor(QObject::tr("#7c8699"));
    QColor grid = QColor(QObject::tr("#d8dee8"));
    QColor title = QColor(QObject::tr("#1d2736"));
};

inline const ChartPalette& chartPalette() {
    static const ChartPalette palette;
    return palette;
}

inline qreal chartFontScale() {
    const QScreen* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        return 1.0;
    }
    return qBound<qreal>(1.0, screen->logicalDotsPerInch() / 96.0, 1.45);
}

inline QFont chartUiFont(int pointSize, bool bold = false) {
    QFont font = QGuiApplication::font();
    font.setBold(bold);
    qreal adjustedPointSize = pointSize;
    qreal scaledPointSize = pointSize * chartFontScale();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    adjustedPointSize += 2.0;
    scaledPointSize = qMax<qreal>(adjustedPointSize, scaledPointSize + 2.0);
    font.setPixelSize(qRound(scaledPointSize * 1.45));
#else
    font.setPointSizeF(qMax<qreal>(adjustedPointSize, scaledPointSize));
#endif
    return font;
}

inline QString wrappedAxisLabel(const QString& rawLabel, int preferredSegmentLength = 8) {
    const QString simplified = rawLabel.trimmed();
    if (simplified.isEmpty() || simplified.size() <= preferredSegmentLength) {
        return simplified;
    }

    QString normalized = simplified;
    normalized.replace(QStringLiteral("/"), QStringLiteral("/ "));
    normalized.replace(QStringLiteral("-"), QStringLiteral("- "));
    normalized.replace(QStringLiteral("_"), QStringLiteral("_ "));

    const QStringList tokens = normalized.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    QStringList lines;
    QString currentLine;
    for (const QString& token : tokens) {
        if (currentLine.isEmpty()) {
            currentLine = token;
            continue;
        }
        if ((currentLine.size() + 1 + token.size()) <= preferredSegmentLength) {
            currentLine += QStringLiteral(" ") + token;
            continue;
        }
        lines.append(currentLine);
        currentLine = token;
    }
    if (!currentLine.isEmpty()) {
        lines.append(currentLine);
    }

    if (lines.size() == 1 && simplified.size() > preferredSegmentLength) {
        QStringList forcedLines;
        for (int index = 0; index < simplified.size(); index += preferredSegmentLength) {
            forcedLines.append(simplified.mid(index, preferredSegmentLength));
        }
        return forcedLines.join(QStringLiteral("\n"));
    }

    return lines.join(QStringLiteral("\n"));
}

inline QString compactAxisLabel(const QString& rawLabel, int preferredSegmentLength = 6) {
    QString normalized = rawLabel.trimmed();
    if (normalized.isEmpty()) {
        return normalized;
    }

    normalized.replace(QStringLiteral("/"), QStringLiteral("\n"));
    normalized.replace(QStringLiteral("-"), QStringLiteral("\n"));
    normalized.replace(QStringLiteral("_"), QStringLiteral("\n"));

    const QStringList parts = normalized.split(QStringLiteral("\n"), Qt::SkipEmptyParts);
    QStringList compactedParts;
    compactedParts.reserve(parts.size());
    for (const QString& part : parts) {
        compactedParts.append(wrappedAxisLabel(part.trimmed(), preferredSegmentLength));
    }
    return compactedParts.join(QStringLiteral("\n"));
}

inline void styleChart(ChartType* chart, const QString& title) {
    if (chart == nullptr) {
        return;
    }

    const auto& palette = chartPalette();
    chart->setTitle(title);
    chart->setAnimationOptions(ChartAnimationOption::SeriesAnimations);
    chart->setBackgroundVisible(true);
    chart->setBackgroundBrush(QBrush(palette.panel));
    chart->setPlotAreaBackgroundVisible(false);
    chart->setMargins(QMargins(42, 16, 36, 44));
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(palette.title);
    chart->legend()->setFont(chartUiFont(12, true));
    chart->setTitleFont(chartUiFont(14, true));
    chart->setTitleBrush(QBrush(palette.title));
}

inline void styleValueAxis(ValueAxisType* axis, const QString& title = QString()) {
    if (axis == nullptr) {
        return;
    }

    const auto& palette = chartPalette();
    axis->setTitleText(title);
    axis->setLabelsColor(palette.title);
    axis->setLinePenColor(palette.axis);
    axis->setGridLineColor(palette.grid);
    axis->setMinorGridLineVisible(false);
    axis->setTitleBrush(QBrush(palette.title));
    axis->setLabelsFont(chartUiFont(11, true));
    axis->setTitleFont(chartUiFont(12, true));
}

inline void styleCategoryAxis(BarCategoryAxisType* axis) {
    if (axis == nullptr) {
        return;
    }

    const auto& palette = chartPalette();
    axis->setLabelsColor(palette.title);
    axis->setLinePenColor(palette.axis);
    axis->setGridLineVisible(false);
    axis->setTitleBrush(QBrush(palette.title));
    axis->setLabelsFont(chartUiFont(11, true));
    axis->setTitleFont(chartUiFont(12, true));
}

inline void styleLineSeries(LineSeriesType* series, const QColor& color) {
    if (series == nullptr) {
        return;
    }

    QPen pen(color, 3);
    pen.setCapStyle(Qt::RoundCap);
    series->setPen(pen);
    series->setPointsVisible(true);
    series->setPointLabelsVisible(false);
}

inline void prepareChartView(ChartViewType* chartView) {
    if (chartView == nullptr) {
        return;
    }

    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setRubberBand(ChartRubberBand::RectangleRubberBand);
    chartView->setMinimumHeight(320);
    chartView->setContextMenuPolicy(Qt::NoContextMenu);
}

inline void refreshPieLegend(ChartType* chart, PieSeriesType* series) {
    if (chart == nullptr || series == nullptr || chart->legend() == nullptr) {
        return;
    }

    const auto& palette = chartPalette();
    const auto markers = chart->legend()->markers(series);
    for (auto* marker : markers) {
        auto* pieMarker = qobject_cast<PieLegendMarkerType*>(marker);
        if (pieMarker == nullptr || pieMarker->slice() == nullptr) {
            continue;
        }

        marker->setVisible(true);
        marker->setLabelBrush(QBrush(palette.axis));
        marker->setFont(chartUiFont(12, true));
        marker->setBrush(QBrush(pieMarker->slice()->color()));
        marker->setPen(QPen(pieMarker->slice()->color().darker(112)));
    }
}

inline void attachPieSliceHoverBehavior(ChartType* chart,
                                        PieSeriesType* series,
                                        PieSliceType* slice,
                                        const QString& tooltipText,
                                        const QColor& baseColor) {
    if (chart == nullptr || series == nullptr || slice == nullptr) {
        return;
    }

    const auto borderColor = baseColor.darker(118);
    slice->setColor(baseColor);
    slice->setBorderColor(borderColor);
    slice->setBorderWidth(1);
    slice->setLabelFont(chartUiFont(11, true));
    slice->setLabelBrush(QBrush(chartPalette().title));

    QObject::connect(slice, &PieSliceType::hovered, chart, [chart, series, slice, tooltipText, baseColor, borderColor](bool status) {
        slice->setExploded(status);
        slice->setColor(status ? baseColor.lighter(112) : baseColor);
        slice->setBorderColor(status ? borderColor.darker(110) : borderColor);
        slice->setBorderWidth(status ? 2 : 1);
        slice->setLabelVisible(false);
        slice->setLabelBrush(QBrush(chartPalette().title));
        if (status) {
            QToolTip::showText(QCursor::pos(), tooltipText);
        }
        refreshPieLegend(chart, series);
    });
}

}  // namespace LaserSpc::Ui
