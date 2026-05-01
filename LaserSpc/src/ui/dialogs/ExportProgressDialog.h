#pragma once

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

namespace LaserSpc::Ui {

class ExportProgressDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExportProgressDialog(QWidget* parent = nullptr);

    void startTask(const QString& title, const QString& detail);
    void finishTask(bool success, const QString& detail);

private:
    void setupUi();
    void refreshBusyText();
    void setProgressValue(int value);

    QString m_baseDetail;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_stateLabel = nullptr;
    QLabel* m_detailLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_closeButton = nullptr;
    QTimer* m_busyTimer = nullptr;
    int m_busyPhase = 0;
    int m_simulatedProgress = 0;
};

}  // namespace LaserSpc::Ui
