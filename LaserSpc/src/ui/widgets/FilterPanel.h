#pragma once

#include <QDateTimeEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

#include "domain/Models.h"

class QComboBox;
class QLabel;
class QToolButton;
class QWidget;

namespace LaserSpc::Ui {

class FilterPanel : public QWidget {
    Q_OBJECT

public:
    explicit FilterPanel(const LaserSpc::Domain::FilterCriteria& defaultCriteria, QWidget* parent = nullptr);

    LaserSpc::Domain::FilterCriteria criteria() const;
    void setCriteria(const LaserSpc::Domain::FilterCriteria& criteria);
    void setDefaultCriteria(const LaserSpc::Domain::FilterCriteria& criteria);
    void setFilterOptions(const LaserSpc::Domain::FilterOptions& options);
    void setEnglish(bool english);

signals:
    void queryRequested(const LaserSpc::Domain::FilterCriteria& criteria);
    void resetRequested(const LaserSpc::Domain::FilterCriteria& criteria);

private:
    void setupUi();
    void setupConnections();
    void refreshFilterChips();
    void removeFilterChip(QWidget* chipWidget);
    void updateAdvancedVisibility();

    LaserSpc::Domain::FilterCriteria m_defaultCriteria;
    QDateTimeEdit* m_beginTimeEdit = nullptr;
    QDateTimeEdit* m_endTimeEdit = nullptr;
    QComboBox* m_lineCombo = nullptr;
    QComboBox* m_programCombo = nullptr;
    QComboBox* m_deviceCombo = nullptr;
    QComboBox* m_resultCombo = nullptr;
    QLineEdit* m_keywordEdit = nullptr;
    QLabel* m_beginTimeLabel = nullptr;
    QLabel* m_endTimeLabel = nullptr;
    QLabel* m_lineLabel = nullptr;
    QLabel* m_programLabel = nullptr;
    QLabel* m_deviceLabel = nullptr;
    QLabel* m_resultLabel = nullptr;
    QLabel* m_keywordLabel = nullptr;
    QWidget* m_timeChipWidget = nullptr;
    QWidget* m_lineChipWidget = nullptr;
    QWidget* m_programChipWidget = nullptr;
    QWidget* m_deviceChipWidget = nullptr;
    QWidget* m_resultChipWidget = nullptr;
    QWidget* m_keywordChipWidget = nullptr;
    QLabel* m_timeChipLabel = nullptr;
    QLabel* m_lineChipLabel = nullptr;
    QLabel* m_programChipLabel = nullptr;
    QLabel* m_deviceChipLabel = nullptr;
    QLabel* m_resultChipLabel = nullptr;
    QLabel* m_keywordChipLabel = nullptr;
    QToolButton* m_lineChipCloseButton = nullptr;
    QToolButton* m_programChipCloseButton = nullptr;
    QToolButton* m_deviceChipCloseButton = nullptr;
    QToolButton* m_resultChipCloseButton = nullptr;
    QToolButton* m_keywordChipCloseButton = nullptr;
    QPushButton* m_queryButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    QPushButton* m_advancedToggleButton = nullptr;
    QWidget* m_formContainer = nullptr;
    QWidget* m_advancedContainer = nullptr;
    QWidget* m_chipContainer = nullptr;
    bool m_english = false;
    bool m_advancedVisible = false;
};

}  // namespace LaserSpc::Ui
