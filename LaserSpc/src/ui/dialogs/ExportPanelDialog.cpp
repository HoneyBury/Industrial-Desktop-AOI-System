#include "ui/dialogs/ExportPanelDialog.h"
#include <QVariant>

#include <QAbstractItemView>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "infrastructure/ExportService.h"
#include "ui/common/UiTheme.h"

namespace LaserSpc::Ui {

namespace {

QString humanReadableSize(qint64 bytes) {
    constexpr qint64 kilo = 1024;
    constexpr qint64 mega = 1024 * 1024;
    if (bytes >= mega) {
        return QString("%1 MB").arg(QString::number(static_cast<double>(bytes) / mega, 'f', 1));
    }
    if (bytes >= kilo) {
        return QString("%1 KB").arg(QString::number(static_cast<double>(bytes) / kilo, 'f', 1));
    }
    return QString("%1 B").arg(bytes);
}

void showNotice(QWidget* parent,
                QMessageBox::Icon icon,
                const QString& title,
                const QString& text,
                const QString& detail = QString()) {
    QMessageBox box(parent);
    box.setIcon(icon);
    box.setWindowTitle(title);
    box.setText(text);
    if (!detail.trimmed().isEmpty()) {
        box.setInformativeText(detail);
    }
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

}  // namespace

ExportPanelDialog::ExportPanelDialog(const QString& currentTaskState,
                                     const QString& reportName,
                                     const QString& reportType,
                                     const QStringList& criteriaSummary,
                                     const QStringList& metricSummary,
                                     QWidget* parent)
    : QDialog(parent),
      m_currentTaskState(currentTaskState),
      m_reportName(reportName),
      m_reportType(reportType),
      m_criteriaSummary(criteriaSummary),
      m_metricSummary(metricSummary) {
    setupUi();
    reloadRecentExports();
}

void ExportPanelDialog::setupUi() {
    setWindowTitle(QObject::tr("导出面板"));
    resize(720, 480);
    UiTheme::applyPanel(this);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 16);
    rootLayout->setSpacing(14);
    auto* titleLabel = new QLabel(QObject::tr("导出面板"), this);
    titleLabel->setProperty("dialogTitle", QVariant(true));
    m_taskStateLabel = new QLabel(m_currentTaskState.isEmpty() ? QObject::tr("当前任务：空闲") : m_currentTaskState, this);
    m_taskStateLabel->setProperty("hint", QVariant(true));

    m_directoryLabel = new QLabel(
        QObject::tr("导出目录：%1").arg(LaserSpc::Infrastructure::ExportService::defaultExportDirectory()), this);
    m_directoryLabel->setWordWrap(true);
    m_directoryLabel->setProperty("hint", QVariant(true));
    m_exportDirectoryEdit = new QLineEdit(this);
    m_exportDirectoryEdit->setPlaceholderText(QObject::tr("留空时使用程序默认目录"));
    m_exportDirectoryEdit->setText(configuredExportRootDirectory());

    m_typeFilterCombo = new QComboBox(this);
    m_typeFilterCombo->addItem(QObject::tr("全部类型"), "");
    m_typeFilterCombo->addItem(QObject::tr("CSV"), ".csv");
    m_typeFilterCombo->addItem(QObject::tr("PNG"), ".png");
    m_typeFilterCombo->addItem(QObject::tr("HTML"), ".html");

    m_recentFilesList = new QListWidget(this);
    m_recentFilesList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto* summaryCard = new QFrame(this);
    UiTheme::applyPanel(summaryCard);
    auto* summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(16, 16, 16, 16);
    summaryLayout->setSpacing(10);
    auto* summaryTitleLabel = new QLabel(QObject::tr("导出目录与筛选"), summaryCard);
    summaryTitleLabel->setProperty("metricTitle", QVariant(true));
    summaryLayout->addWidget(summaryTitleLabel);
    summaryLayout->addWidget(m_taskStateLabel);
    summaryLayout->addWidget(m_directoryLabel);
    auto* directoryEditorLayout = new QHBoxLayout();
    m_browseDirectoryButton = new QPushButton(QObject::tr("浏览目录"), this);
    m_saveDirectoryButton = new QPushButton(QObject::tr("保存目录"), this);
    UiTheme::applySecondaryButton(m_browseDirectoryButton);
    UiTheme::applyPrimaryButton(m_saveDirectoryButton);
    directoryEditorLayout->addWidget(m_exportDirectoryEdit, 1);
    directoryEditorLayout->addWidget(m_browseDirectoryButton);
    directoryEditorLayout->addWidget(m_saveDirectoryButton);
    summaryLayout->addLayout(directoryEditorLayout);
    summaryLayout->addWidget(m_typeFilterCombo);

    auto* listCard = new QFrame(this);
    UiTheme::applyPanel(listCard);
    auto* listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(16, 16, 16, 16);
    listLayout->setSpacing(12);
    auto* listTitleLabel = new QLabel(QObject::tr("最近导出记录"), listCard);
    listTitleLabel->setProperty("metricTitle", QVariant(true));

    auto* actionLayout = new QHBoxLayout();
    m_refreshButton = new QPushButton(QObject::tr("刷新记录"), this);
    m_openDirectoryButton = new QPushButton(QObject::tr("打开目录"), this);
    m_openFileButton = new QPushButton(QObject::tr("打开文件"), this);
    m_reportBundleButton = new QPushButton(QObject::tr("生成报告包"), this);
    m_removeFileButton = new QPushButton(QObject::tr("删除选中"), this);
    UiTheme::applySecondaryButton(m_refreshButton);
    UiTheme::applyPrimaryButton(m_openDirectoryButton);
    UiTheme::applySecondaryButton(m_openFileButton);
    UiTheme::applyPrimaryButton(m_reportBundleButton);
    UiTheme::applySecondaryButton(m_removeFileButton);
    m_refreshButton->setMinimumWidth(92);
    m_openDirectoryButton->setMinimumWidth(92);
    m_openFileButton->setMinimumWidth(92);
    m_reportBundleButton->setMinimumWidth(108);
    m_removeFileButton->setMinimumWidth(92);
    actionLayout->addWidget(m_refreshButton);
    actionLayout->addWidget(m_openDirectoryButton);
    actionLayout->addWidget(m_openFileButton);
    actionLayout->addWidget(m_reportBundleButton);
    actionLayout->addWidget(m_removeFileButton);
    actionLayout->addStretch();

    auto* pagerLayout = new QHBoxLayout();
    m_prevPageButton = new QPushButton(QObject::tr("上一页"), this);
    m_nextPageButton = new QPushButton(QObject::tr("下一页"), this);
    m_pageInfoLabel = new QLabel(QObject::tr("第 1 页 / 共 1 页"), this);
    m_pageInfoLabel->setProperty("hint", QVariant(true));
    UiTheme::applySecondaryButton(m_prevPageButton);
    UiTheme::applySecondaryButton(m_nextPageButton);
    pagerLayout->addStretch();
    pagerLayout->addWidget(m_prevPageButton);
    pagerLayout->addWidget(m_pageInfoLabel);
    pagerLayout->addWidget(m_nextPageButton);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    UiTheme::applySecondaryButton(buttonBox->button(QDialogButtonBox::Close));
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    listLayout->addWidget(listTitleLabel);
    listLayout->addLayout(actionLayout);
    listLayout->addWidget(m_recentFilesList, 1);
    listLayout->addLayout(pagerLayout);

    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(summaryCard);
    rootLayout->addWidget(listCard, 1);
    rootLayout->addWidget(buttonBox);

    connect(m_refreshButton, &QPushButton::clicked, this, &ExportPanelDialog::reloadRecentExports);
    connect(m_browseDirectoryButton, &QPushButton::clicked, this, &ExportPanelDialog::browseExportDirectory);
    connect(m_saveDirectoryButton, &QPushButton::clicked, this, &ExportPanelDialog::saveExportDirectory);
    connect(m_openDirectoryButton, &QPushButton::clicked, this, &ExportPanelDialog::openExportDirectory);
    connect(m_openFileButton, &QPushButton::clicked, this, &ExportPanelDialog::openSelectedExportFile);
    connect(m_reportBundleButton, &QPushButton::clicked, this, &ExportPanelDialog::createReportBundle);
    connect(m_removeFileButton, &QPushButton::clicked, this, &ExportPanelDialog::removeSelectedExportFiles);
    connect(m_typeFilterCombo, &QComboBox::currentTextChanged, this, &ExportPanelDialog::reloadRecentExports);
    connect(m_prevPageButton, &QPushButton::clicked, this, [this]() { setCurrentPage(m_currentPage - 1); });
    connect(m_nextPageButton, &QPushButton::clicked, this, [this]() { setCurrentPage(m_currentPage + 1); });
    connect(m_recentFilesList, &QListWidget::itemSelectionChanged, this, &ExportPanelDialog::refreshSelectionState);
    connect(m_recentFilesList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { openSelectedExportFile(); });
}

void ExportPanelDialog::reloadRecentExports() {
    m_filteredFiles.clear();

    const auto files = LaserSpc::Infrastructure::ExportService::recentExportFiles(200);
    const QString extensionFilter = selectedExtensionFilter();
    for (const QFileInfo& file : files) {
        if (!extensionFilter.isEmpty() && file.suffix().compare(extensionFilter.mid(1), Qt::CaseInsensitive) != 0) {
            continue;
        }
        m_filteredFiles.append(file);
    }

    m_currentPage = 1;
    refreshCurrentPage();
}

void ExportPanelDialog::refreshCurrentPage() {
    if (m_recentFilesList == nullptr) {
        return;
    }

    m_recentFilesList->clear();
    if (m_filteredFiles.isEmpty()) {
        m_recentFilesList->addItem(selectedExtensionFilter().isEmpty() ? QObject::tr("暂无导出记录。")
                                                                      : QObject::tr("当前筛选条件下暂无导出记录。"));
        m_pageInfoLabel->setText(QObject::tr("第 1 页 / 共 1 页 | 0 条记录"));
        m_prevPageButton->setEnabled(false);
        m_nextPageButton->setEnabled(false);
        refreshSelectionState();
        return;
    }

    m_currentPage = qBound(1, m_currentPage, totalPages());
    const int begin = (m_currentPage - 1) * m_pageSize;
    const int end = qMin(begin + m_pageSize, m_filteredFiles.size());
    for (int index = begin; index < end; ++index) {
        const QFileInfo& file = m_filteredFiles.at(index);
        auto* item = new QListWidgetItem(
            QObject::tr("%1 | %2 | %3 | %4")
                .arg(file.lastModified().toString("yyyy-MM-dd HH:mm:ss"))
                .arg(file.fileName())
                .arg(file.suffix().toUpper())
                .arg(humanReadableSize(file.size())),
            m_recentFilesList);
        item->setData(Qt::UserRole, file.absoluteFilePath());
    }

    if (m_recentFilesList->count() > 0 && m_recentFilesList->selectedItems().isEmpty()) {
        m_recentFilesList->setCurrentRow(0);
    }

    m_pageInfoLabel->setText(QObject::tr("第 %1 页 / 共 %2 页 | %3 条记录")
                                 .arg(m_currentPage)
                                 .arg(totalPages())
                                 .arg(m_filteredFiles.size()));
    m_prevPageButton->setEnabled(m_currentPage > 1);
    m_nextPageButton->setEnabled(m_currentPage < totalPages());
    refreshSelectionState();
}

void ExportPanelDialog::refreshSelectionState() {
    const bool hasSelection = !selectedFilePaths().isEmpty();
    m_openFileButton->setEnabled(hasSelection);
    m_reportBundleButton->setEnabled(hasSelection);
    m_removeFileButton->setEnabled(hasSelection);
}

void ExportPanelDialog::openExportDirectory() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(LaserSpc::Infrastructure::ExportService::defaultExportDirectory()));
}

void ExportPanelDialog::openSelectedExportFile() {
    const QStringList paths = selectedFilePaths();
    if (paths.isEmpty()) {
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(paths.first()));
}

void ExportPanelDialog::browseExportDirectory() {
    const QString selected = QFileDialog::getExistingDirectory(
        this,
        QObject::tr("选择导出目录"),
        m_exportDirectoryEdit->text().trimmed().isEmpty()
            ? LaserSpc::Infrastructure::ExportService::exportRootDirectory()
            : m_exportDirectoryEdit->text().trimmed());
    if (!selected.isEmpty()) {
        m_exportDirectoryEdit->setText(QDir::toNativeSeparators(selected));
    }
}

void ExportPanelDialog::saveExportDirectory() {
    LaserSpc::Infrastructure::AppConfigService configService;
    auto settings = configService.settings();
    settings.exportDirectory = m_exportDirectoryEdit->text().trimmed();

    QString errorMessage;
    if (!configService.saveSettings(settings, &errorMessage)) {
        showNotice(this,
                   QMessageBox::Warning,
                   QObject::tr("保存导出目录失败"),
                   QObject::tr("导出目录没有保存成功。"),
                   errorMessage);
        return;
    }

    m_directoryLabel->setText(
        QObject::tr("导出目录：%1").arg(LaserSpc::Infrastructure::ExportService::defaultExportDirectory()));
    reloadRecentExports();
    showNotice(this,
               QMessageBox::Information,
               QObject::tr("导出目录已更新"),
               QObject::tr("新的导出目录设置已经生效。"),
               LaserSpc::Infrastructure::ExportService::defaultExportDirectory());
}

void ExportPanelDialog::createReportBundle() {
    const QStringList paths = selectedFilePaths();
    if (paths.isEmpty()) {
        return;
    }

    LaserSpc::Infrastructure::ExportReportBundleOptions options;
    LaserSpc::Infrastructure::AppConfigService configService;
    options.reportName = m_reportName.isEmpty() ? QObject::tr("spc_report") : m_reportName;
    options.reportType = m_reportType.isEmpty() ? m_typeFilterCombo->currentText() : m_reportType;
    options.currentTaskState = m_currentTaskState;
    options.selectedFiles = paths;
    options.criteriaSummary = m_criteriaSummary;
    options.metricSummary = m_metricSummary;
    options.templateSettings = configService.reportTemplateSettings();
    options.notes = QStringList{
        QObject::tr("来源目录：%1").arg(LaserSpc::Infrastructure::ExportService::defaultExportDirectory()),
        QObject::tr("筛选类型：%1").arg(m_typeFilterCombo->currentText())
    };

    QString outputPath;
    QString errorMessage;
    if (!LaserSpc::Infrastructure::ExportService::createReportBundle(options, &outputPath, &errorMessage)) {
        showNotice(this,
                   QMessageBox::Warning,
                   QObject::tr("生成报告包失败"),
                   QObject::tr("报告包生成失败，请检查导出目录和文件权限。"),
                   errorMessage);
        return;
    }

    showNotice(this,
               QMessageBox::Information,
               QObject::tr("报告包已生成"),
               QObject::tr("导出报告包生成完成。"),
               outputPath);
    reloadRecentExports();
}

void ExportPanelDialog::removeSelectedExportFiles() {
    const QStringList paths = selectedFilePaths();
    if (paths.isEmpty()) {
        return;
    }

    if (QMessageBox::question(this,
                              QObject::tr("确认删除"),
                              QObject::tr("确定删除选中的 %1 个导出文件吗？").arg(paths.size()),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    if (!LaserSpc::Infrastructure::ExportService::removeExportFiles(paths, &errorMessage)) {
        showNotice(this,
                   QMessageBox::Warning,
                   QObject::tr("删除导出文件失败"),
                   QObject::tr("选中的导出文件没有删除成功。"),
                   errorMessage);
        return;
    }

    reloadRecentExports();
    showNotice(this,
               QMessageBox::Information,
               QObject::tr("删除完成"),
               QObject::tr("选中的导出文件已删除。"),
               QObject::tr("已删除 %1 个文件。").arg(paths.size()));
}

QString ExportPanelDialog::selectedExtensionFilter() const {
    return m_typeFilterCombo == nullptr ? QString() : m_typeFilterCombo->currentData().toString();
}

QStringList ExportPanelDialog::selectedFilePaths() const {
    QStringList paths;
    if (m_recentFilesList == nullptr) {
        return paths;
    }

    const auto selectedItems = m_recentFilesList->selectedItems();
    for (QListWidgetItem* item : selectedItems) {
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    return paths;
}

int ExportPanelDialog::totalPages() const {
    return qMax(1, (m_filteredFiles.size() + m_pageSize - 1) / m_pageSize);
}

void ExportPanelDialog::setCurrentPage(int page) {
    m_currentPage = page;
    refreshCurrentPage();
}

QString ExportPanelDialog::configuredExportRootDirectory() const {
    const auto settings = LaserSpc::Infrastructure::AppConfigService().settings();
    return settings.exportDirectory.trimmed();
}

}  // namespace LaserSpc::Ui
