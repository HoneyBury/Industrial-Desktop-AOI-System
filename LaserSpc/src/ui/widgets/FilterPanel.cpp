#include "ui/widgets/FilterPanel.h"
#include <QVariant>

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "ui/common/UiTextCatalog.h"
#include "ui/common/UiTheme.h"

namespace LaserSpc::Ui {

namespace {

QString allStoredValue() {
    return TextCatalog::allSelection();
}

QString allDisplayValue(bool english) {
    return english ? QObject::tr("All") : allStoredValue();
}

QString normalizeAllStoredValue(const QString& value) {
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() || trimmed == QObject::tr("All") ? allStoredValue() : trimmed;
}

QString comboStoredValue(const QComboBox* combo) {
    const QVariant data = combo->currentData();
    return normalizeAllStoredValue(data.isValid() ? data.toString() : combo->currentText());
}

void syncAllOptionLabel(QComboBox* combo, bool english) {
    if (combo->count() > 0) {
        combo->setItemText(0, allDisplayValue(english));
    }
}

void refreshComboOptions(QComboBox* combo, const QStringList& options, const QString& preferredValue, bool english) {
    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(allDisplayValue(english), allStoredValue());
    for (const QString& option : options) {
        const QString normalized = normalizeAllStoredValue(option);
        if (!normalized.isEmpty() && normalized != allStoredValue()) {
            combo->addItem(normalized, normalized);
        }
    }

    QString valueToApply = normalizeAllStoredValue(preferredValue);
    if (valueToApply.isEmpty()) {
        valueToApply = allStoredValue();
    }
    if (combo->findData(valueToApply) < 0) {
        combo->addItem(valueToApply, valueToApply);
    }
    combo->setCurrentIndex(combo->findData(valueToApply));
}

QWidget* createChipWidget(QWidget* parent, QLabel** labelRef, QToolButton** buttonRef, bool removable) {
    auto* chip = new QWidget(parent);
    chip->setProperty("chip", QVariant(true));
    auto* layout = new QHBoxLayout(chip);
    layout->setContentsMargins(10, 4, 8, 4);
    layout->setSpacing(6);

    auto* label = new QLabel(chip);
    label->setProperty("chipText", QVariant(true));
    layout->addWidget(label);

    if (buttonRef != nullptr) {
        *buttonRef = nullptr;
    }
    if (removable) {
        auto* button = new QToolButton(chip);
        button->setAutoRaise(true);
        button->setText(QObject::tr("×"));
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setFixedSize(18, 18);
        layout->addWidget(button);
        if (buttonRef != nullptr) {
            *buttonRef = button;
        }
    }

    chip->setVisible(false);
    if (labelRef != nullptr) {
        *labelRef = label;
    }
    return chip;
}

}  // namespace

FilterPanel::FilterPanel(const LaserSpc::Domain::FilterCriteria& defaultCriteria, QWidget* parent)
    : QWidget(parent), m_defaultCriteria(defaultCriteria) {
    setupUi();
    setCriteria(defaultCriteria);
    setupConnections();
}

void FilterPanel::setupUi() {
    UiTheme::applyPanel(this);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 10, 12, 10);
    rootLayout->setSpacing(10);

    auto* topLayout = new QVBoxLayout();
    topLayout->setSpacing(10);

    m_formContainer = new QWidget(this);
    auto* formLayout = new QGridLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(10);
    formLayout->setVerticalSpacing(8);

    m_beginTimeEdit = new QDateTimeEdit(this);
    m_beginTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_beginTimeEdit->setCalendarPopup(true);
    m_beginTimeEdit->setMinimumWidth(180);

    m_endTimeEdit = new QDateTimeEdit(this);
    m_endTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_endTimeEdit->setCalendarPopup(true);
    m_endTimeEdit->setMinimumWidth(180);

    m_lineCombo = new QComboBox(this);
    m_programCombo = new QComboBox(this);
    m_deviceCombo = new QComboBox(this);
    m_lineCombo->setMinimumWidth(120);
    m_programCombo->setMinimumWidth(140);
    m_deviceCombo->setMinimumWidth(140);

    m_resultCombo = new QComboBox(this);
    m_resultCombo->addItem(allDisplayValue(false), allStoredValue());
    m_resultCombo->addItem(QObject::tr("OK"), QStringLiteral("OK"));
    m_resultCombo->addItem(QObject::tr("NG"), QStringLiteral("NG"));
    m_resultCombo->setMinimumWidth(100);

    m_keywordEdit = new QLineEdit(this);
    m_keywordEdit->setMinimumWidth(180);

    auto addField = [formLayout](int row, int column, QLabel** labelRef, QWidget* editor) {
        auto* label = new QLabel();
        label->setProperty("hint", QVariant(true));
        *labelRef = label;
        formLayout->addWidget(label, row, column * 2);
        formLayout->addWidget(editor, row, column * 2 + 1);
    };

    addField(0, 0, &m_beginTimeLabel, m_beginTimeEdit);
    addField(0, 1, &m_endTimeLabel, m_endTimeEdit);
    addField(0, 2, &m_lineLabel, m_lineCombo);
    addField(0, 3, &m_programLabel, m_programCombo);

    m_advancedContainer = new QWidget(this);
    auto* advancedLayout = new QGridLayout(m_advancedContainer);
    advancedLayout->setContentsMargins(0, 0, 0, 0);
    advancedLayout->setHorizontalSpacing(10);
    advancedLayout->setVerticalSpacing(8);

    auto addAdvancedField = [advancedLayout](int row, int column, QLabel** labelRef, QWidget* editor) {
        auto* label = new QLabel();
        label->setProperty("hint", QVariant(true));
        *labelRef = label;
        advancedLayout->addWidget(label, row, column * 2);
        advancedLayout->addWidget(editor, row, column * 2 + 1);
    };

    addAdvancedField(0, 0, &m_deviceLabel, m_deviceCombo);
    addAdvancedField(0, 1, &m_resultLabel, m_resultCombo);
    addAdvancedField(0, 2, &m_keywordLabel, m_keywordEdit);

    auto* formStackLayout = new QVBoxLayout();
    formStackLayout->setContentsMargins(0, 0, 0, 0);
    formStackLayout->setSpacing(8);
    formStackLayout->addLayout(formLayout);
    formStackLayout->addWidget(m_advancedContainer);
    m_formContainer->setLayout(formStackLayout);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);
    m_advancedToggleButton = new QPushButton(this);
    m_queryButton = new QPushButton(this);
    m_resetButton = new QPushButton(this);
    UiTheme::applySecondaryButton(m_advancedToggleButton);
    UiTheme::applyPrimaryButton(m_queryButton);
    UiTheme::applySecondaryButton(m_resetButton);
    m_advancedToggleButton->setMinimumWidth(96);
    m_advancedToggleButton->setMaximumWidth(112);
    m_queryButton->setMinimumWidth(78);
    m_queryButton->setMaximumWidth(88);
    m_resetButton->setMinimumWidth(78);
    m_resetButton->setMaximumWidth(88);
    m_advancedToggleButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_queryButton->setIcon(style()->standardIcon(QStyle::SP_CommandLink));
    m_resetButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    actionLayout->addStretch();
    actionLayout->addWidget(m_advancedToggleButton);
    actionLayout->addWidget(m_queryButton);
    actionLayout->addWidget(m_resetButton);

    topLayout->addWidget(m_formContainer);
    topLayout->addLayout(actionLayout);

    m_chipContainer = new QWidget(this);
    auto* chipLayout = new QHBoxLayout(m_chipContainer);
    chipLayout->setContentsMargins(0, 0, 0, 0);
    chipLayout->setSpacing(8);
    m_timeChipWidget = createChipWidget(m_chipContainer, &m_timeChipLabel, nullptr, false);
    m_lineChipWidget = createChipWidget(m_chipContainer, &m_lineChipLabel, &m_lineChipCloseButton, true);
    m_programChipWidget = createChipWidget(m_chipContainer, &m_programChipLabel, &m_programChipCloseButton, true);
    m_deviceChipWidget = createChipWidget(m_chipContainer, &m_deviceChipLabel, &m_deviceChipCloseButton, true);
    m_resultChipWidget = createChipWidget(m_chipContainer, &m_resultChipLabel, &m_resultChipCloseButton, true);
    m_keywordChipWidget = createChipWidget(m_chipContainer, &m_keywordChipLabel, &m_keywordChipCloseButton, true);
    chipLayout->addWidget(m_timeChipWidget);
    chipLayout->addWidget(m_lineChipWidget);
    chipLayout->addWidget(m_programChipWidget);
    chipLayout->addWidget(m_deviceChipWidget);
    chipLayout->addWidget(m_resultChipWidget);
    chipLayout->addWidget(m_keywordChipWidget);
    chipLayout->addStretch();

    rootLayout->addLayout(topLayout);
    rootLayout->addWidget(m_chipContainer);

    setFilterOptions(LaserSpc::Domain::FilterOptions{});
    setEnglish(false);
    updateAdvancedVisibility();
}

void FilterPanel::setupConnections() {
    connect(m_queryButton, &QPushButton::clicked, this, [this]() {
        refreshFilterChips();
        emit queryRequested(criteria());
    });

    connect(m_resetButton, &QPushButton::clicked, this, [this]() {
        setCriteria(m_defaultCriteria);
        emit resetRequested(criteria());
    });

    connect(m_advancedToggleButton, &QPushButton::clicked, this, [this]() {
        m_advancedVisible = !m_advancedVisible;
        updateAdvancedVisibility();
    });

    auto refreshChips = [this]() { refreshFilterChips(); };
    connect(m_beginTimeEdit, &QDateTimeEdit::dateTimeChanged, this, refreshChips);
    connect(m_endTimeEdit, &QDateTimeEdit::dateTimeChanged, this, refreshChips);
    connect(m_lineCombo, &QComboBox::currentTextChanged, this, refreshChips);
    connect(m_programCombo, &QComboBox::currentTextChanged, this, refreshChips);
    connect(m_deviceCombo, &QComboBox::currentTextChanged, this, refreshChips);
    connect(m_resultCombo, &QComboBox::currentTextChanged, this, refreshChips);
    connect(m_keywordEdit, &QLineEdit::textChanged, this, refreshChips);

    connect(m_lineChipCloseButton, &QToolButton::clicked, this, [this]() { removeFilterChip(m_lineChipWidget); });
    connect(m_programChipCloseButton, &QToolButton::clicked, this, [this]() { removeFilterChip(m_programChipWidget); });
    connect(m_deviceChipCloseButton, &QToolButton::clicked, this, [this]() { removeFilterChip(m_deviceChipWidget); });
    connect(m_resultChipCloseButton, &QToolButton::clicked, this, [this]() { removeFilterChip(m_resultChipWidget); });
    connect(m_keywordChipCloseButton, &QToolButton::clicked, this, [this]() { removeFilterChip(m_keywordChipWidget); });
}

LaserSpc::Domain::FilterCriteria FilterPanel::criteria() const {
    LaserSpc::Domain::FilterCriteria value;
    value.beginTime = m_beginTimeEdit->dateTime();
    value.endTime = m_endTimeEdit->dateTime();
    value.lineName = comboStoredValue(m_lineCombo);
    value.programName = comboStoredValue(m_programCombo);
    value.deviceName = comboStoredValue(m_deviceCombo);
    value.result = comboStoredValue(m_resultCombo);
    value.keyword = m_keywordEdit->text().trimmed();
    return value;
}

void FilterPanel::setCriteria(const LaserSpc::Domain::FilterCriteria& criteria) {
    m_beginTimeEdit->setDateTime(criteria.beginTime);
    m_endTimeEdit->setDateTime(criteria.endTime);

    const auto applyComboValue = [](QComboBox* combo, const QString& value) {
        const QString normalized = normalizeAllStoredValue(value);
        const int index = combo->findData(normalized);
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
    };

    applyComboValue(m_lineCombo, criteria.lineName);
    applyComboValue(m_programCombo, criteria.programName);
    applyComboValue(m_deviceCombo, criteria.deviceName);
    applyComboValue(m_resultCombo, criteria.result);
    m_keywordEdit->setText(criteria.keyword);
    refreshFilterChips();
}

void FilterPanel::setDefaultCriteria(const LaserSpc::Domain::FilterCriteria& criteria) {
    m_defaultCriteria = criteria;
}

void FilterPanel::setFilterOptions(const LaserSpc::Domain::FilterOptions& options) {
    const auto currentCriteria = criteria();
    refreshComboOptions(m_lineCombo, options.lineNames, currentCriteria.lineName, m_english);
    refreshComboOptions(m_programCombo, options.programNames, currentCriteria.programName, m_english);
    refreshComboOptions(m_deviceCombo, options.deviceNames, currentCriteria.deviceName, m_english);
    refreshFilterChips();
}

void FilterPanel::setEnglish(bool english) {
    m_english = english;

    m_beginTimeLabel->setText(english ? QObject::tr("Start") : QObject::tr("开始时间"));
    m_endTimeLabel->setText(english ? QObject::tr("End") : QObject::tr("结束时间"));
    m_lineLabel->setText(english ? QObject::tr("Line") : QObject::tr("线体"));
    m_programLabel->setText(english ? QObject::tr("Program") : QObject::tr("程序"));
    m_deviceLabel->setText(english ? QObject::tr("Device") : QObject::tr("设备"));
    m_resultLabel->setText(english ? QObject::tr("Result") : QObject::tr("结果"));
    m_keywordLabel->setText(english ? QObject::tr("Keyword") : QObject::tr("关键字"));
    m_queryButton->setText(english ? QObject::tr("Search") : QObject::tr("查询"));
    m_resetButton->setText(english ? QObject::tr("Reset") : QObject::tr("重置"));
    m_advancedToggleButton->setText(m_advancedVisible
                                        ? (english ? QObject::tr("Hide advanced") : QObject::tr("收起高级"))
                                        : (english ? QObject::tr("Advanced") : QObject::tr("高级筛选")));
    m_keywordEdit->setPlaceholderText(english ? QObject::tr("Board / point / operator")
                                              : QObject::tr("板号 / 点位 / 人员关键字"));
    syncAllOptionLabel(m_lineCombo, english);
    syncAllOptionLabel(m_programCombo, english);
    syncAllOptionLabel(m_deviceCombo, english);
    syncAllOptionLabel(m_resultCombo, english);
    refreshFilterChips();
}

void FilterPanel::refreshFilterChips() {
    if (m_timeChipLabel == nullptr || m_lineChipLabel == nullptr || m_programChipLabel == nullptr || m_deviceChipLabel == nullptr ||
        m_resultChipLabel == nullptr || m_keywordChipLabel == nullptr) {
        return;
    }

    const auto current = criteria();
    const QString all = allStoredValue();

    m_timeChipLabel->setText((m_english ? QObject::tr("Time: ") : QObject::tr("时间：")) +
                             current.beginTime.toString("MM-dd HH:mm") + QObject::tr(" ~ ") +
                             current.endTime.toString("MM-dd HH:mm"));
    m_timeChipWidget->setVisible(true);

    auto applyChip = [](QWidget* chip, QLabel* label, const QString& prefix, const QString& value, const QString& emptyValue = QString()) {
        const QString trimmed = value.trimmed();
        const bool visible = !trimmed.isEmpty() && trimmed != emptyValue;
        chip->setVisible(visible);
        if (visible) {
            label->setText(prefix + trimmed);
        }
    };

    applyChip(m_lineChipWidget, m_lineChipLabel, m_english ? QObject::tr("Line: ") : QObject::tr("线体："), current.lineName, all);
    applyChip(m_programChipWidget, m_programChipLabel, m_english ? QObject::tr("Program: ") : QObject::tr("程序："), current.programName, all);
    applyChip(m_deviceChipWidget, m_deviceChipLabel, m_english ? QObject::tr("Device: ") : QObject::tr("设备："), current.deviceName, all);
    applyChip(m_resultChipWidget, m_resultChipLabel, m_english ? QObject::tr("Result: ") : QObject::tr("结果："), current.result, all);
    applyChip(m_keywordChipWidget, m_keywordChipLabel, m_english ? QObject::tr("Keyword: ") : QObject::tr("关键字："), current.keyword);
}

void FilterPanel::removeFilterChip(QWidget* chipWidget) {
    if (chipWidget == nullptr) {
        return;
    }

    const QString allValue = allStoredValue();
    if (chipWidget == m_lineChipWidget) {
        const int index = m_lineCombo->findData(allValue);
        if (index >= 0) {
            m_lineCombo->setCurrentIndex(index);
        }
    } else if (chipWidget == m_programChipWidget) {
        const int index = m_programCombo->findData(allValue);
        if (index >= 0) {
            m_programCombo->setCurrentIndex(index);
        }
    } else if (chipWidget == m_deviceChipWidget) {
        const int index = m_deviceCombo->findData(allValue);
        if (index >= 0) {
            m_deviceCombo->setCurrentIndex(index);
        }
    } else if (chipWidget == m_resultChipWidget) {
        const int index = m_resultCombo->findData(allValue);
        if (index >= 0) {
            m_resultCombo->setCurrentIndex(index);
        }
    } else if (chipWidget == m_keywordChipWidget) {
        m_keywordEdit->clear();
    } else {
        return;
    }

    refreshFilterChips();
    emit queryRequested(criteria());
}

void FilterPanel::updateAdvancedVisibility() {
    m_advancedContainer->setVisible(m_advancedVisible);
    m_advancedToggleButton->setText(m_advancedVisible
                                        ? (m_english ? QObject::tr("Hide advanced") : QObject::tr("收起高级"))
                                        : (m_english ? QObject::tr("Advanced") : QObject::tr("高级筛选")));
}

}  // namespace LaserSpc::Ui
