#include "ui/widgets/ResultToolbar.h"
#include <QVariant>

#include <QComboBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>

#include "ui/common/UiTheme.h"

namespace LaserSpc::Ui {

ResultToolbar::ResultToolbar(const QString& csvButtonText,
                             const QString& imageButtonText,
                             QWidget* parent)
    : QWidget(parent) {
    setupUi(csvButtonText, imageButtonText);
}

void ResultToolbar::setupUi(const QString& csvButtonText, const QString& imageButtonText) {
    UiTheme::applyPanel(this);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    m_exportCsvButton = new QPushButton(csvButtonText, this);
    m_exportImageButton = new QPushButton(imageButtonText, this);
    m_exportPanelButton = new QPushButton(this);
    UiTheme::applyPrimaryButton(m_exportCsvButton);
    UiTheme::applySecondaryButton(m_exportImageButton);
    UiTheme::applySecondaryButton(m_exportPanelButton);

    m_pageSizeLabel = new QLabel(this);
    m_pageSizeLabel->setProperty("hint", QVariant(true));
    m_pageSizeCombo = new QComboBox(this);
    m_pageSizeCombo->addItems({QStringLiteral("10"), QStringLiteral("20"), QStringLiteral("50")});
    m_pageSizeCombo->setCurrentText(QStringLiteral("10"));
    m_pageSizeCombo->setMinimumWidth(76);

    m_prevPageButton = new QPushButton(this);
    m_nextPageButton = new QPushButton(this);
    m_jumpLabel = new QLabel(this);
    m_jumpLabel->setProperty("hint", QVariant(true));
    m_pageJumpSpin = new QSpinBox(this);
    m_pageJumpSpin->setMinimum(1);
    m_pageJumpSpin->setMaximum(1);
    m_pageJumpSpin->setMinimumWidth(76);
    m_jumpPageButton = new QPushButton(this);

    m_pageInfoLabel = new QLabel(this);
    m_pageInfoLabel->setProperty("hint", QVariant(true));

    m_taskStateLabel = new QLabel(this);
    m_taskStateLabel->setProperty("hint", QVariant(true));
    m_taskStateLabel->setWordWrap(false);
    m_taskStateLabel->setMinimumWidth(0);
    m_taskStateLabel->setContentsMargins(10, 6, 10, 6);
    m_taskStateLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    UiTheme::applyStatusLabel(m_taskStateLabel, StatusTone::Neutral);

    layout->addWidget(m_exportCsvButton);
    layout->addWidget(m_exportImageButton);
    layout->addWidget(m_exportPanelButton);
    layout->addSpacing(10);
    layout->addWidget(m_pageSizeLabel);
    layout->addWidget(m_pageSizeCombo);
    layout->addSpacing(6);
    layout->addWidget(m_prevPageButton);
    layout->addWidget(m_nextPageButton);
    layout->addWidget(m_jumpLabel);
    layout->addWidget(m_pageJumpSpin);
    layout->addWidget(m_jumpPageButton);
    layout->addWidget(m_pageInfoLabel);
    layout->addSpacing(8);
    layout->addWidget(m_taskStateLabel, 1);

    connect(m_exportCsvButton, &QPushButton::clicked, this, &ResultToolbar::exportCsvRequested);
    connect(m_exportImageButton, &QPushButton::clicked, this, &ResultToolbar::exportImageRequested);
    connect(m_exportPanelButton, &QPushButton::clicked, this, &ResultToolbar::exportPanelRequested);
    connect(m_prevPageButton, &QPushButton::clicked, this, &ResultToolbar::previousPageRequested);
    connect(m_nextPageButton, &QPushButton::clicked, this, &ResultToolbar::nextPageRequested);
    connect(m_jumpPageButton, &QPushButton::clicked, this, [this]() { emit pageJumpRequested(m_pageJumpSpin->value()); });
    connect(m_pageSizeCombo, &QComboBox::currentTextChanged, this, [this](const QString& value) {
        emit pageSizeChanged(value.toInt());
    });

    setTexts(false, csvButtonText, imageButtonText);
}

int ResultToolbar::pageSize() const {
    return m_pageSizeCombo->currentText().toInt();
}

void ResultToolbar::setCurrentPage(int page) {
    m_currentPage = qMax(1, page);
    refreshPageLabel();
}

void ResultToolbar::setTotalRows(int totalRows) {
    m_totalRows = qMax(0, totalRows);
    refreshPageLabel();
}

void ResultToolbar::setPageSize(int pageSize) {
    if (pageSize <= 0) {
        return;
    }
    m_pageSizeCombo->setCurrentText(QString::number(pageSize));
    refreshPageLabel();
}

QString ResultToolbar::idleTaskState() const {
    return m_english ? tr("Export task: idle") : tr("导出任务：空闲");
}

void ResultToolbar::setTexts(bool english, const QString& csvButtonText, const QString& imageButtonText) {
    const QString previousIdle = idleTaskState();
    m_english = english;
    m_exportCsvButton->setText(csvButtonText);
    m_exportImageButton->setText(imageButtonText);
    m_exportPanelButton->setText(english ? tr("Export Panel") : tr("导出面板"));
    m_pageSizeLabel->setText(english ? tr("Page size") : tr("每页"));
    m_prevPageButton->setText(english ? tr("Prev") : tr("上一页"));
    m_nextPageButton->setText(english ? tr("Next") : tr("下一页"));
    m_jumpLabel->setText(english ? tr("Jump to") : tr("跳至"));
    m_jumpPageButton->setText(english ? tr("Go") : tr("跳转"));
    if (m_taskStateMessage.isEmpty() || m_taskStateMessage == previousIdle) {
        m_taskStateMessage = idleTaskState();
    }
    refreshPageLabel();
    updateTaskStateLabel();
}

void ResultToolbar::setExportControlsEnabled(bool enabled) {
    if (m_exportCsvButton != nullptr) {
        m_exportCsvButton->setEnabled(enabled);
    }
    if (m_exportImageButton != nullptr) {
        m_exportImageButton->setEnabled(enabled);
    }
    if (m_exportPanelButton != nullptr) {
        m_exportPanelButton->setEnabled(enabled);
    }
}

void ResultToolbar::setTaskState(const QString& message, bool isError) {
    m_taskStateMessage = message.isEmpty() ? idleTaskState() : message;
    UiTheme::applyStatusLabel(m_taskStateLabel, isError ? StatusTone::Danger : StatusTone::Neutral);
    updateTaskStateLabel();
}

QString ResultToolbar::taskState() const {
    return m_taskStateMessage;
}

void ResultToolbar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateTaskStateLabel();
}

void ResultToolbar::refreshPageLabel() {
    const int size = qMax(1, pageSize());
    const int totalPages = qMax(1, (m_totalRows + size - 1) / size);
    const int currentPage = qMin(qMax(1, m_currentPage), totalPages);
    const QSignalBlocker blocker(m_pageJumpSpin);
    m_pageJumpSpin->setMaximum(totalPages);
    m_pageJumpSpin->setValue(currentPage);

    m_pageInfoLabel->setText(m_english ? tr("Page %1 / %2").arg(currentPage).arg(totalPages)
                                       : tr("第 %1 页 / 共 %2 页").arg(currentPage).arg(totalPages));
    m_prevPageButton->setEnabled(currentPage > 1);
    m_nextPageButton->setEnabled(currentPage < totalPages);
    m_jumpPageButton->setEnabled(totalPages > 1);
}

void ResultToolbar::updateTaskStateLabel() {
    if (m_taskStateLabel == nullptr) {
        return;
    }

    const int availableWidth = qMax(120, width() / 3);
    const QString elided = fontMetrics().elidedText(m_taskStateMessage, Qt::ElideRight, availableWidth);
    m_taskStateLabel->setText(elided);
    m_taskStateLabel->setToolTip(elided == m_taskStateMessage ? QString() : m_taskStateMessage);
}

}  // namespace LaserSpc::Ui
