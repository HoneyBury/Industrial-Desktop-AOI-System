#include "ui/common/UiTheme.h"
#include <QVariant>

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QWidget>

namespace LaserSpc::Ui {

namespace {

QString qtCompatibilityOverrides() {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return QStringLiteral(R"(
QWidget {
    font-size: 14px;
}
QPushButton, QLineEdit, QComboBox, QDateTimeEdit, QSpinBox, QListWidget, QTableWidget {
    font-size: 14px;
}
QHeaderView::section {
    font-size: 14px;
}
QLabel[sectionTitle="true"] {
    font-size: 27px;
}
QLabel[metricValue="true"] {
    font-size: 34px;
}
QLabel[compactMetricValue="true"] {
    font-size: 22px;
}
QLabel[dialogTitle="true"] {
    font-size: 25px;
}
QLabel[metricTitle="true"],
QLabel[chip="true"],
QLabel[metricTrend="true"] {
    font-size: 14px;
}
)");
#else
    return {};
#endif
}

struct ThemePalette {
    QString appBackground;
    QString textPrimary;
    QString panelBackground;
    QString panelBorder;
    QString accent;
    QString accentHover;
    QString accentSoft;
    QString accentSoftBorder;
    QString inputBorder;
    QString listHover;
    QString listSelected;
    QString headerBackground;
    QString tableGrid;
    QString tableAlt;
    QString selectionBackground;
    QString selectionText;
    QString chartBorder;
    QString statusSuccess;
    QString statusWarning;
    QString statusDanger;
    QString metricTitle;
    QString metricSurface;
};

ThemePalette paletteFor(LaserSpc::Infrastructure::ThemeStyle themeStyle) {
    using LaserSpc::Infrastructure::ThemeStyle;

    switch (themeStyle) {
        case ThemeStyle::Night:
            return {
                QStringLiteral("#0b1118"), QStringLiteral("#d6e2f1"), QStringLiteral("#131c26"), QStringLiteral("#243446"),
                QStringLiteral("#3d7cff"), QStringLiteral("#5690ff"), QStringLiteral("#152235"), QStringLiteral("#28466f"),
                QStringLiteral("#31465f"), QStringLiteral("#5f97ff"), QStringLiteral("#142131"), QStringLiteral("#28415d"),
                QStringLiteral("#163152"), QStringLiteral("#172535"), QStringLiteral("#16212e"), QStringLiteral("#243446"),
                QStringLiteral("#101924"), QStringLiteral("#1b3657"), QStringLiteral("#edf4ff"), QStringLiteral("#2c4867"),
                QStringLiteral("#9bb7d7"), QStringLiteral("#0f1722")
            };
        case ThemeStyle::Graphite:
            return {
                QStringLiteral("#eef1f5"), QStringLiteral("#18212b"), QStringLiteral("#ffffff"), QStringLiteral("#cfd6df"),
                QStringLiteral("#2f5d7c"), QStringLiteral("#3c6f91"), QStringLiteral("#e7eef4"), QStringLiteral("#c2cfdb"),
                QStringLiteral("#c4ced9"), QStringLiteral("#edf2f7"), QStringLiteral("#dbe4ee"), QStringLiteral("#e5ebf2"),
                QStringLiteral("#d8dee6"), QStringLiteral("#f7f9fb"), QStringLiteral("#d8e6ef"), QStringLiteral("#162635"),
                QStringLiteral("#d2dae3"), QStringLiteral("#1f6f43"), QStringLiteral("#9a6700"), QStringLiteral("#b42318")
            };
        case ThemeStyle::Sand:
            return {
                QStringLiteral("#f8f4ec"), QStringLiteral("#2c2117"), QStringLiteral("#fffdf8"), QStringLiteral("#dfd3bf"),
                QStringLiteral("#9b5d28"), QStringLiteral("#b06a32"), QStringLiteral("#f7e8d4"), QStringLiteral("#e1bf95"),
                QStringLiteral("#d6c2a5"), QStringLiteral("#faf1e2"), QStringLiteral("#f2dfc7"), QStringLiteral("#efe4d3"),
                QStringLiteral("#e2d3bf"), QStringLiteral("#fff9f1"), QStringLiteral("#f8e4c8"), QStringLiteral("#402718"),
                QStringLiteral("#d7c6af"), QStringLiteral("#3b7a3b"), QStringLiteral("#a46d00"), QStringLiteral("#b54708"),
                QStringLiteral("#7a4b25"), QStringLiteral("#fff5e9")
            };
        case ThemeStyle::Forest:
            return {
                QStringLiteral("#eef5f0"), QStringLiteral("#1c2b21"), QStringLiteral("#fbfefb"), QStringLiteral("#cad7cc"),
                QStringLiteral("#2f6f4f"), QStringLiteral("#3d8660"), QStringLiteral("#e5f1ea"), QStringLiteral("#b8d2c0"),
                QStringLiteral("#c5d3c8"), QStringLiteral("#edf5ef"), QStringLiteral("#dcebdc"), QStringLiteral("#e4eee5"),
                QStringLiteral("#d4dfd5"), QStringLiteral("#f7fbf7"), QStringLiteral("#d8ebde"), QStringLiteral("#183122"),
                QStringLiteral("#ccd8cf"), QStringLiteral("#2d7a46"), QStringLiteral("#9a6700"), QStringLiteral("#b42318"),
                QStringLiteral("#35624b"), QStringLiteral("#f2faf4")
            };
        case ThemeStyle::Ember:
            return {
                QStringLiteral("#fbf2ee"), QStringLiteral("#311f1a"), QStringLiteral("#fffdfc"), QStringLiteral("#e1cfc8"),
                QStringLiteral("#b5522f"), QStringLiteral("#c8643f"), QStringLiteral("#f8e5de"), QStringLiteral("#e8bfaf"),
                QStringLiteral("#d8c2b8"), QStringLiteral("#fbefe9"), QStringLiteral("#f3ddd4"), QStringLiteral("#f1e2dc"),
                QStringLiteral("#e5d0c8"), QStringLiteral("#fff7f3"), QStringLiteral("#fae3d9"), QStringLiteral("#4a2a20"),
                QStringLiteral("#dfc7be"), QStringLiteral("#2f7d4c"), QStringLiteral("#b26a00"), QStringLiteral("#c2410c"),
                QStringLiteral("#8e4227"), QStringLiteral("#fff3ed")
            };
        case ThemeStyle::Aurora:
            return {
                QStringLiteral("#f2f4fb"), QStringLiteral("#1d2240"), QStringLiteral("#ffffff"), QStringLiteral("#d3d8ef"),
                QStringLiteral("#4c5bd4"), QStringLiteral("#6573e0"), QStringLiteral("#e8ebfb"), QStringLiteral("#c6cef4"),
                QStringLiteral("#c8d0ec"), QStringLiteral("#eef1fd"), QStringLiteral("#dde3fa"), QStringLiteral("#e8ecfb"),
                QStringLiteral("#dde2f3"), QStringLiteral("#f8f9ff"), QStringLiteral("#e0e6ff"), QStringLiteral("#1b2452"),
                QStringLiteral("#d3d8eb"), QStringLiteral("#2f7d4c"), QStringLiteral("#a66b00"), QStringLiteral("#b42318"),
                QStringLiteral("#4451b4"), QStringLiteral("#f3f5ff")
            };
        case ThemeStyle::Ocean:
        default:
            return {
                QStringLiteral("#f3f6fb"), QStringLiteral("#182230"), QStringLiteral("#ffffff"), QStringLiteral("#d8dee8"),
                QStringLiteral("#0b4f6c"), QStringLiteral("#0d5d7f"), QStringLiteral("#eaf2f6"), QStringLiteral("#bfd1da"),
                QStringLiteral("#c6d0dd"), QStringLiteral("#eef5f8"), QStringLiteral("#dbe9f0"), QStringLiteral("#e9eff5"),
                QStringLiteral("#e4e7ec"), QStringLiteral("#f9fbfc"), QStringLiteral("#d9ebf2"), QStringLiteral("#102a43"),
                QStringLiteral("#d8dee8"), QStringLiteral("#1f6f43"), QStringLiteral("#9a6700"), QStringLiteral("#b42318"),
                QStringLiteral("#426b80"), QStringLiteral("#f6fbfd")
            };
    }
}

QString themeSpecificOverrides(LaserSpc::Infrastructure::ThemeStyle themeStyle, const ThemePalette& palette) {
    using LaserSpc::Infrastructure::ThemeStyle;
    if (themeStyle != ThemeStyle::Night) {
        return {};
    }

    return QStringLiteral(R"(
QWidget[panel="true"] QLabel[hint="true"],
QFrame[panel="true"] QLabel[hint="true"],
QLabel[hint="true"],
QLabel[metricDesc="true"],
QLabel[statusTone="neutral"] {
    color: #93a8bf;
}
QLabel[statusTone="success"] {
    color: #5fd0a5;
}
QLabel[statusTone="warning"] {
    color: #f3b45f;
}
QLabel[statusTone="danger"] {
    color: #ff8a80;
}
QPushButton {
    background: #16202b;
    color: %1;
    border-color: #31465f;
}
QPushButton:hover {
    border-color: %2;
    background: #1a2633;
}
QPushButton:disabled {
    color: #6f8196;
    background: #101720;
    border-color: #223243;
}
QLineEdit, QComboBox, QDateTimeEdit, QSpinBox {
    background: #111923;
    color: %1;
    border-color: #31465f;
}
QComboBox::drop-down {
    background: #162231;
    border-left-color: #28415d;
}
QComboBox QAbstractItemView {
    background: #101924;
    color: %1;
    border-color: #243446;
    selection-background-color: #173253;
    selection-color: #edf4ff;
}
QTabBar::tab {
    background: #132030;
    color: #9bb7d7;
    border-color: #28415d;
}
QTabBar::tab:selected {
    background: #182433;
    color: #dce9ff;
}
QListWidget::item:selected {
    background: #163152;
    color: #dce9ff;
}
QHeaderView::section {
    background: #16212e;
    color: #cfe0f5;
}
QTableWidget {
    background: #111923;
    color: %1;
    border-color: #243446;
    alternate-background-color: #101924;
    gridline-color: #243446;
    selection-background-color: #1b3657;
    selection-color: #edf4ff;
}
QStatusBar {
    background: #0f1722;
    border-top-color: #243446;
}
QChartView {
    border-color: #2c4867;
}
QToolTip {
    background: #152235;
    color: #edf4ff;
    border: 1px solid #28466f;
    border-radius: 10px;
    padding: 8px 10px;
    font-size: 14px;
    font-weight: 600;
}
)")
        .arg(palette.textPrimary, palette.accent);
}

}  // namespace

QString UiTheme::applicationStyleSheet(LaserSpc::Infrastructure::ThemeStyle themeStyle) {
    const ThemePalette palette = paletteFor(themeStyle);

    const QString baseStyle = QObject::tr(R"(
QWidget {
    background: %1;
    color: %2;
    font-family: "Microsoft YaHei UI";
    font-size: 13px;
}
QMainWindow::separator {
    background: %3;
    width: 1px;
    height: 1px;
}
QFrame[panel="true"], QWidget[panel="true"], QGroupBox[panel="true"] {
    background: %4;
    border: 1px solid %3;
    border-radius: 12px;
}
QGroupBox[panel="true"] {
    margin-top: 12px;
    padding-top: 14px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 4px;
    color: %21;
}
QFrame[metricCard="true"], QGroupBox[metricCard="true"] {
    background: %22;
    border: 1px solid %3;
    border-radius: 14px;
}
QFrame[compactMetricCard="true"] {
    background: %22;
    border: 1px solid %3;
    border-radius: 12px;
}
QWidget[panel="true"] QLabel[statusTone], QFrame[panel="true"] QLabel[statusTone] {
    padding: 2px 0;
}
QLabel[hint="true"] {
    color: #475467;
    background: transparent;
}
QLabel[statusTone="neutral"] {
    color: #667085;
    background: transparent;
}
QLabel[statusTone="success"] {
    color: %5;
    background: transparent;
}
QLabel[statusTone="warning"] {
    color: %6;
    background: transparent;
}
QLabel[statusTone="danger"] {
    color: %7;
    background: transparent;
}
QLabel[sectionTitle="true"] {
    font-size: 24px;
    font-weight: 700;
    color: %2;
    background: transparent;
}
QLabel[metricValue="true"] {
    font-size: 30px;
    font-weight: 700;
    color: %8;
    background: transparent;
}
QLabel[compactMetricValue="true"] {
    font-size: 20px;
    font-weight: 700;
    color: %2;
    background: transparent;
}
QLabel[metricTitle="true"] {
    color: %21;
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 0.5px;
    text-transform: uppercase;
    background: transparent;
}
QLabel[metricDesc="true"] {
    color: #667085;
    background: transparent;
}
QLabel[dialogTitle="true"] {
    font-size: 22px;
    font-weight: 700;
    color: %2;
    background: transparent;
}
QLabel[formLabel="true"] {
    color: %21;
    font-weight: 600;
    background: transparent;
}
QLabel[chip="true"] {
    background: %11;
    color: %8;
    border: 1px solid %12;
    border-radius: 999px;
    padding: 4px 10px;
    font-size: 12px;
    font-weight: 600;
}
QLabel[metricTrend="true"] {
    color: %8;
    background: transparent;
    font-size: 12px;
    font-weight: 600;
}
QLabel[emptyStateTitle="true"] {
    font-size: 20px;
    font-weight: 700;
    color: %2;
    background: transparent;
}
QLabel[emptyStateDesc="true"] {
    color: #667085;
    background: transparent;
    max-width: 420px;
}
QLabel[hint="true"], QLabel[metricDesc="true"] {
    line-height: 1.25em;
}
QPushButton {
    min-height: 34px;
    padding: 0 14px;
    border-radius: 8px;
    border: 1px solid %9;
    background: #ffffff;
}
QPushButton:hover {
    border-color: %8;
}
QPushButton:disabled {
    color: #98a2b3;
    background: #f2f4f7;
    border-color: #eaecf0;
}
QPushButton[variant="primary"] {
    background: %8;
    color: #ffffff;
    border-color: %8;
    font-weight: 600;
}
QPushButton[variant="primary"]:hover {
    background: %10;
}
QPushButton[variant="secondary"] {
    background: %11;
    color: %8;
    border-color: %12;
}
QPushButton[configLauncher="true"] {
    min-width: 122px;
    padding-right: 24px;
}
QLineEdit, QComboBox, QDateTimeEdit, QSpinBox {
    min-height: 34px;
    padding: 0 10px;
    background: #ffffff;
    border: 1px solid %9;
    border-radius: 8px;
}
QLineEdit:focus, QComboBox:focus, QDateTimeEdit:focus, QSpinBox:focus {
    border-color: %8;
}
QComboBox {
    padding: 0 36px 0 12px;
}
QComboBox:hover {
    border-color: %12;
}
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 28px;
    border-left: 1px solid %12;
    background: %11;
    border-top-right-radius: 8px;
    border-bottom-right-radius: 8px;
}
QComboBox::drop-down:hover {
    background: %14;
}
QComboBox QAbstractItemView {
    background: #ffffff;
    border: 1px solid %3;
    border-radius: 10px;
    padding: 6px;
    selection-background-color: %13;
    selection-color: %8;
    outline: 0;
}
QComboBox QAbstractItemView::item {
    min-height: 28px;
    padding: 6px 10px;
    margin: 2px 0;
    border-radius: 8px;
}
QComboBox QAbstractItemView::item:hover {
    background: %14;
    color: %2;
}
QCheckBox {
    spacing: 8px;
    background: transparent;
}
QProgressBar {
    min-height: 6px;
    max-height: 6px;
    border: none;
    border-radius: 999px;
    background: %11;
}
QProgressBar::chunk {
    border-radius: 999px;
    background: %8;
}
QProgressBar[exportProgress="true"] {
    min-height: 22px;
    max-height: 22px;
    padding: 0;
    border: 1px solid %12;
    border-radius: 999px;
    background: %11;
    color: %8;
    text-align: center;
    font-weight: 700;
}
QProgressBar[exportProgress="true"]::chunk {
    border-radius: 999px;
    background: %8;
}
QTabWidget::pane {
    border: 1px solid %3;
    border-radius: 12px;
    top: -1px;
    background: %4;
}
QTabBar::tab {
    background: %11;
    border: 1px solid %12;
    border-bottom: none;
    padding: 8px 14px;
    margin-right: 6px;
    border-top-left-radius: 9px;
    border-top-right-radius: 9px;
    color: %21;
}
QTabBar::tab:selected {
    background: %4;
    color: %8;
    font-weight: 600;
}
QTabBar::tab:!selected:hover {
    background: %14;
}
QListWidget {
    background: transparent;
    border: none;
    outline: none;
}
QListWidget::item {
    margin: 3px 0;
    padding: 11px 12px;
    border-radius: 12px;
}
QListWidget::item:selected {
    background: %13;
    color: %8;
    font-weight: 600;
}
QListWidget::item:hover {
    background: %14;
}
QHeaderView::section {
    background: %15;
    color: #344054;
    padding: 8px;
    border: none;
    border-bottom: 1px solid %3;
    border-right: 1px solid %3;
    font-weight: 600;
}
QTableWidget {
    background: #ffffff;
    border: 1px solid %3;
    border-radius: 10px;
    gridline-color: %16;
    alternate-background-color: %17;
    selection-background-color: %18;
    selection-color: %19;
}
QTableWidget::item {
    padding: 6px;
}
QChartView {
    background: transparent;
    border: 1px solid %20;
    border-radius: 10px;
}
QToolTip {
    background: %4;
    color: %2;
    border: 1px solid %12;
    border-radius: 10px;
    padding: 8px 10px;
    font-size: 14px;
    font-weight: 600;
}
QMenu {
    background: #ffffff;
    color: %2;
    border: 1px solid %3;
    border-radius: 10px;
    padding: 6px;
}
QMenu::item {
    padding: 8px 14px;
    border-radius: 8px;
}
QMenu::item:selected {
    background: %14;
    color: %8;
}
QSplitter::handle {
    background: transparent;
}
QSplitter::handle:horizontal {
    width: 10px;
}
QSplitter::handle:vertical {
    height: 10px;
}
QStatusBar {
    background: #ffffff;
    border-top: 1px solid %3;
}
)")
        .arg(palette.appBackground,
             palette.textPrimary,
             palette.panelBorder,
             palette.panelBackground,
             palette.statusSuccess,
             palette.statusWarning,
             palette.statusDanger,
             palette.accent,
             palette.inputBorder,
             palette.accentHover,
             palette.accentSoft,
             palette.accentSoftBorder,
             palette.listSelected,
             palette.listHover,
             palette.headerBackground,
             palette.tableGrid,
             palette.tableAlt,
             palette.selectionBackground,
             palette.selectionText,
             palette.chartBorder,
             palette.metricTitle,
             palette.metricSurface);

    return baseStyle + themeSpecificOverrides(themeStyle, palette) + qtCompatibilityOverrides();
}

void UiTheme::applyPanel(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }
    widget->setProperty("panel", QVariant(true));
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

void UiTheme::applyTable(QTableWidget* table) {
    if (table == nullptr) {
        return;
    }
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->setShowGrid(true);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
}

void UiTheme::applyPrimaryButton(QPushButton* button) {
    if (button == nullptr) {
        return;
    }
    button->setProperty("variant", "primary");
    button->style()->unpolish(button);
    button->style()->polish(button);
}

void UiTheme::applySecondaryButton(QPushButton* button) {
    if (button == nullptr) {
        return;
    }
    button->setProperty("variant", "secondary");
    button->style()->unpolish(button);
    button->style()->polish(button);
}

void UiTheme::applyStatusLabel(QLabel* label, StatusTone tone) {
    if (label == nullptr) {
        return;
    }

    switch (tone) {
        case StatusTone::Success:
            label->setProperty("statusTone", "success");
            break;
        case StatusTone::Warning:
            label->setProperty("statusTone", "warning");
            break;
        case StatusTone::Danger:
            label->setProperty("statusTone", "danger");
            break;
        case StatusTone::Neutral:
        default:
            label->setProperty("statusTone", "neutral");
            break;
    }
    label->style()->unpolish(label);
    label->style()->polish(label);
}

void UiTheme::applyPopupMenu(QMenu* menu) {
    if (menu == nullptr) {
        return;
    }
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background: #ffffff; color: #182230; border: 1px solid #d8dee8; }"
        "QMenu::item { color: #182230; background: transparent; padding: 8px 14px; }"
        "QMenu::item:selected { color: #0b4f6c; background: #eef5f8; }"));
}

}  // namespace LaserSpc::Ui
