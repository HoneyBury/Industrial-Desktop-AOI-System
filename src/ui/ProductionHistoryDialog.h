#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "database/DatabaseManager.h"

#include <QDialog>
#include <QString>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextEdit;

class ProductionHistoryDialog final : public QDialog {
  Q_OBJECT

public:
  explicit ProductionHistoryDialog(DatabaseManager *databaseManager,
                                   QWidget *parent = nullptr);

  void refreshData();

private:
  void buildUi();
  void applyFilter();
  void showDetail(int row);
  void exportToCsv();

  DatabaseManager *databaseManager_;

  // Summary cards
  QLabel *totalCountLabel_ {nullptr};
  QLabel *okCountLabel_ {nullptr};
  QLabel *ngCountLabel_ {nullptr};
  QLabel *yieldLabel_ {nullptr};

  // Filter
  QComboBox *decisionFilterComboBox_ {nullptr};
  QLineEdit *programFilterEdit_ {nullptr};

  // Table
  QTableWidget *resultTable_ {nullptr};

  // Export
  QPushButton *exportButton_ {nullptr};

  // Detail
  QTextEdit *detailTextEdit_ {nullptr};

  QVector<InspectionRecord> currentRecords_;
};

#endif
