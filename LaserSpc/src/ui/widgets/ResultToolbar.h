#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QResizeEvent;
class QSpinBox;

namespace LaserSpc::Ui {

class ResultToolbar : public QWidget {
    Q_OBJECT

public:
    explicit ResultToolbar(const QString& csvButtonText,
                           const QString& imageButtonText,
                           QWidget* parent = nullptr);

    int pageSize() const;
    void setCurrentPage(int page);
    void setTotalRows(int totalRows);
    void setPageSize(int pageSize);
    void setTexts(bool english, const QString& csvButtonText, const QString& imageButtonText);
    void setExportControlsEnabled(bool enabled);
    void setTaskState(const QString& message, bool isError = false);
    QString taskState() const;

signals:
    void exportCsvRequested();
    void exportImageRequested();
    void exportPanelRequested();
    void previousPageRequested();
    void nextPageRequested();
    void pageJumpRequested(int page);
    void pageSizeChanged(int pageSize);

private:
    void resizeEvent(QResizeEvent* event) override;
    void setupUi(const QString& csvButtonText, const QString& imageButtonText);
    void refreshPageLabel();
    void updateTaskStateLabel();
    QString idleTaskState() const;

    QPushButton* m_exportCsvButton = nullptr;
    QPushButton* m_exportImageButton = nullptr;
    QPushButton* m_exportPanelButton = nullptr;
    QLabel* m_pageSizeLabel = nullptr;
    QComboBox* m_pageSizeCombo = nullptr;
    QPushButton* m_prevPageButton = nullptr;
    QPushButton* m_nextPageButton = nullptr;
    QLabel* m_jumpLabel = nullptr;
    QSpinBox* m_pageJumpSpin = nullptr;
    QPushButton* m_jumpPageButton = nullptr;
    QLabel* m_pageInfoLabel = nullptr;
    QLabel* m_taskStateLabel = nullptr;

    int m_currentPage = 1;
    int m_totalRows = 0;
    QString m_taskStateMessage;
    bool m_english = false;
};

}  // namespace LaserSpc::Ui
