#include "ui/dialogs/DiagnosticsDialog.h"
#include <QVariant>

#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "ui/common/UiTheme.h"

namespace LaserSpc::Ui {

DiagnosticsDialog::DiagnosticsDialog(const LaserSpc::Infrastructure::RuntimeDiagnosticsSnapshot& snapshot,
                                     bool exportReportEnabled,
                                     QWidget* parent)
    : QDialog(parent), m_snapshot(snapshot), m_exportReportEnabled(exportReportEnabled) {
    setupUi();
}

void DiagnosticsDialog::setupUi() {
    setWindowTitle(QObject::tr("运行诊断"));
    resize(720, 520);
    UiTheme::applyPanel(this);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 16);
    rootLayout->setSpacing(14);

    auto* titleLabel = new QLabel(QObject::tr("运行诊断"), this);
    titleLabel->setProperty("dialogTitle", QVariant(true));
    m_summaryLabel = new QLabel(
        QObject::tr("当前共发现 %1 条运行告警，可导出诊断报告用于部署排查。").arg(m_snapshot.warnings.size()), this);
    m_summaryLabel->setProperty("hint", QVariant(true));
    m_summaryLabel->setWordWrap(true);

    m_configPathLabel = new QLabel(QObject::tr("配置文件：%1").arg(m_snapshot.configFilePath), this);
    m_exportDirectoryLabel = new QLabel(QObject::tr("导出目录：%1").arg(m_snapshot.exportDirectory), this);
    m_fontDirectoryLabel = new QLabel(QObject::tr("Qt 字体目录：%1").arg(m_snapshot.qtFontDirectory), this);
    m_pluginDirectoryLabel = new QLabel(QObject::tr("Qt 插件目录：%1").arg(m_snapshot.qtPluginDirectory), this);
    m_driverLabel = new QLabel(QObject::tr("SQL 驱动：%1").arg(m_snapshot.sqlDrivers.join(QObject::tr(", "))), this);

    for (QLabel* label : {m_configPathLabel, m_exportDirectoryLabel, m_fontDirectoryLabel, m_pluginDirectoryLabel, m_driverLabel}) {
        label->setWordWrap(true);
        label->setProperty("hint", QVariant(true));
    }

    m_warningList = new QListWidget(this);
    if (m_snapshot.warnings.isEmpty()) {
        m_warningList->addItem(QObject::tr("未发现运行时告警。"));
    } else {
        m_warningList->addItems(m_snapshot.warnings);
    }

    m_exportReportButton = new QPushButton(QObject::tr("导出诊断报告"), this);
    UiTheme::applySecondaryButton(m_exportReportButton);
    m_exportReportButton->setMinimumWidth(136);
    m_exportReportButton->setMaximumWidth(160);
    m_exportReportButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_exportReportButton->setEnabled(m_exportReportEnabled);
    connect(m_exportReportButton, &QPushButton::clicked, this, &DiagnosticsDialog::exportReport);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    UiTheme::applySecondaryButton(buttonBox->button(QDialogButtonBox::Close));
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* summaryCard = new QFrame(this);
    UiTheme::applyPanel(summaryCard);
    auto* summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(16, 16, 16, 16);
    summaryLayout->setSpacing(8);
    summaryLayout->addWidget(m_summaryLabel);
    summaryLayout->addWidget(m_configPathLabel);
    summaryLayout->addWidget(m_exportDirectoryLabel);
    summaryLayout->addWidget(m_fontDirectoryLabel);
    summaryLayout->addWidget(m_pluginDirectoryLabel);
    summaryLayout->addWidget(m_driverLabel);

    auto* warningCard = new QFrame(this);
    UiTheme::applyPanel(warningCard);
    auto* warningLayout = new QVBoxLayout(warningCard);
    warningLayout->setContentsMargins(16, 16, 16, 16);
    warningLayout->setSpacing(12);
    warningLayout->addWidget(m_warningList, 1);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->addStretch();
    actionLayout->addWidget(m_exportReportButton);
    actionLayout->addWidget(buttonBox);

    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(summaryCard);
    rootLayout->addWidget(warningCard, 1);
    rootLayout->addLayout(actionLayout);
}

void DiagnosticsDialog::exportReport() {
    QString outputPath;
    QString errorMessage;
    if (!LaserSpc::Infrastructure::RuntimeDiagnostics::exportSnapshotReport(m_snapshot, &outputPath, &errorMessage)) {
        QMessageBox::warning(this, QObject::tr("导出失败"), errorMessage);
        return;
    }

    QMessageBox::information(this, QObject::tr("导出成功"), QObject::tr("诊断报告已生成：%1").arg(outputPath));
}

}  // namespace LaserSpc::Ui
