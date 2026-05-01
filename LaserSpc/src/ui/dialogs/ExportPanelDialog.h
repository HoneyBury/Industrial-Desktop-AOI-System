#pragma once

#include <QDialog>
#include <QFileInfo>
#include <QStringList>

class QLabel;
class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace LaserSpc::Ui {

class ExportPanelDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExportPanelDialog(const QString& currentTaskState,
                               const QString& reportName = QString(),
                               const QString& reportType = QString(),
                               const QStringList& criteriaSummary = {},
                               const QStringList& metricSummary = {},
                               QWidget* parent = nullptr);

private:
    void setupUi();
    void reloadRecentExports();
    void refreshCurrentPage();
    void refreshSelectionState();
    void openExportDirectory();
    void openSelectedExportFile();
    void browseExportDirectory();
    void saveExportDirectory();
    void createReportBundle();
    void removeSelectedExportFiles();
    QString selectedExtensionFilter() const;
    QStringList selectedFilePaths() const;
    int totalPages() const;
    void setCurrentPage(int page);
    QString configuredExportRootDirectory() const;

    QString m_currentTaskState;
    QString m_reportName;
    QString m_reportType;
    QStringList m_criteriaSummary;
    QStringList m_metricSummary;
    QLabel* m_taskStateLabel = nullptr;
    QLabel* m_directoryLabel = nullptr;
    QLabel* m_pageInfoLabel = nullptr;
    QLineEdit* m_exportDirectoryEdit = nullptr;
    QComboBox* m_typeFilterCombo = nullptr;
    QListWidget* m_recentFilesList = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_browseDirectoryButton = nullptr;
    QPushButton* m_saveDirectoryButton = nullptr;
    QPushButton* m_openDirectoryButton = nullptr;
    QPushButton* m_openFileButton = nullptr;
    QPushButton* m_reportBundleButton = nullptr;
    QPushButton* m_removeFileButton = nullptr;
    QPushButton* m_prevPageButton = nullptr;
    QPushButton* m_nextPageButton = nullptr;
    QList<QFileInfo> m_filteredFiles;
    int m_currentPage = 1;
    int m_pageSize = 12;
};

}  // namespace LaserSpc::Ui
