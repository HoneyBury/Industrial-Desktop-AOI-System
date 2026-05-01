#include "ui/dialogs/ExportProgressDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

#include "ui/common/UiTheme.h"

namespace LaserSpc::Ui {

ExportProgressDialog::ExportProgressDialog(QWidget* parent) : QDialog(parent) {
    setupUi();
}

void ExportProgressDialog::setupUi() {
    setModal(false);
    setWindowTitle(QObject::tr("导出任务"));
    setMinimumWidth(480);
    setSizeGripEnabled(false);
    UiTheme::applyPanel(this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 18);
    layout->setSpacing(14);

    m_titleLabel = new QLabel(QObject::tr("导出任务"), this);
    m_titleLabel->setProperty("dialogTitle", QVariant(true));

    m_stateLabel = new QLabel(QObject::tr("正在准备导出"), this);
    m_stateLabel->setProperty("metricTitle", QVariant(true));

    m_detailLabel = new QLabel(QObject::tr("正在初始化导出上下文。"), this);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setProperty("hint", QVariant(true));

    m_progressBar = new QProgressBar(this);
    m_progressBar->setProperty("exportProgress", QVariant(true));
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(QObject::tr("%p%"));
    setProgressValue(6);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_closeButton = buttonBox->button(QDialogButtonBox::Close);
    UiTheme::applySecondaryButton(m_closeButton);
    m_closeButton->setEnabled(false);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::hide);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_stateLabel);
    layout->addWidget(m_detailLabel);
    layout->addWidget(m_progressBar);
    layout->addWidget(buttonBox);

    m_busyTimer = new QTimer(this);
    m_busyTimer->setInterval(260);
    connect(m_busyTimer, &QTimer::timeout, this, &ExportProgressDialog::refreshBusyText);
}

void ExportProgressDialog::startTask(const QString& title, const QString& detail) {
    m_titleLabel->setText(title);
    m_stateLabel->setText(QObject::tr("正在导出"));
    m_baseDetail = detail;
    m_busyPhase = 0;
    m_simulatedProgress = 6;
    setProgressValue(m_simulatedProgress);
    m_closeButton->setEnabled(false);
    UiTheme::applyStatusLabel(m_detailLabel, StatusTone::Neutral);
    refreshBusyText();
    m_busyTimer->start();
    show();
    raise();
    activateWindow();
}

void ExportProgressDialog::finishTask(bool success, const QString& detail) {
    m_busyTimer->stop();
    setProgressValue(100);
    m_stateLabel->setText(success ? QObject::tr("导出完成") : QObject::tr("导出失败"));
    m_closeButton->setEnabled(true);
    UiTheme::applyStatusLabel(m_detailLabel, success ? StatusTone::Success : StatusTone::Danger);
    m_detailLabel->setText(detail);
    show();
    raise();
    activateWindow();
}

void ExportProgressDialog::refreshBusyText() {
    const QString dots = QStringLiteral(".").repeated((m_busyPhase % 3) + 1);
    m_detailLabel->setText(m_baseDetail + dots);
    if (m_simulatedProgress < 92) {
        const int step = m_simulatedProgress < 40 ? 7 : (m_simulatedProgress < 72 ? 4 : 2);
        m_simulatedProgress = qMin(92, m_simulatedProgress + step);
        setProgressValue(m_simulatedProgress);
    }
    ++m_busyPhase;
}

void ExportProgressDialog::setProgressValue(int value) {
    if (m_progressBar == nullptr) {
        return;
    }
    m_progressBar->setValue(qBound(0, value, 100));
}

}  // namespace LaserSpc::Ui
