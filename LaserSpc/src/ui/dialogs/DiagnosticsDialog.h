#pragma once

#include <QDialog>

#include "infrastructure/RuntimeDiagnostics.h"

class QLabel;
class QListWidget;
class QPushButton;

namespace LaserSpc::Ui {

class DiagnosticsDialog : public QDialog {
    Q_OBJECT

public:
    explicit DiagnosticsDialog(const LaserSpc::Infrastructure::RuntimeDiagnosticsSnapshot& snapshot,
                               bool exportReportEnabled = true,
                               QWidget* parent = nullptr);

private:
    void setupUi();
    void exportReport();

    LaserSpc::Infrastructure::RuntimeDiagnosticsSnapshot m_snapshot;
    bool m_exportReportEnabled = true;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_configPathLabel = nullptr;
    QLabel* m_exportDirectoryLabel = nullptr;
    QLabel* m_fontDirectoryLabel = nullptr;
    QLabel* m_pluginDirectoryLabel = nullptr;
    QLabel* m_driverLabel = nullptr;
    QListWidget* m_warningList = nullptr;
    QPushButton* m_exportReportButton = nullptr;
};

}  // namespace LaserSpc::Ui
