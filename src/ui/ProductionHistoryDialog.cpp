#include "ui/ProductionHistoryDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

ProductionHistoryDialog::ProductionHistoryDialog(DatabaseManager *databaseManager,
                                                 QWidget *parent)
    : QDialog(parent), databaseManager_(databaseManager) {
  setWindowTitle(QStringLiteral("生产统计与历史查询"));
  resize(1100, 750);
  buildUi();
  refreshData();
}

void ProductionHistoryDialog::buildUi() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(20, 20, 20, 20);
  rootLayout->setSpacing(14);

  // ── Summary cards ──
  auto *summaryGroup = new QGroupBox(QStringLiteral("生产概览"), this);
  summaryGroup->setStyleSheet(QStringLiteral(
      "QGroupBox { color: #e2e8f0; font-weight: 600; border: 1px solid #334155; "
      "border-radius: 8px; margin-top: 10px; padding-top: 14px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 12px; }"));
  auto *summaryLayout = new QHBoxLayout(summaryGroup);
  summaryLayout->setSpacing(16);

  auto makeCard = [this](const QString &title, const QString &color) {
    auto *card = new QFrame(this);
    card->setStyleSheet(QStringLiteral(
        "QFrame { background: #0f172a; border: 1px solid #334155; border-radius: 10px; }"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(4);
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 11px;"));
    auto *valueLabel = new QLabel(QStringLiteral("—"), card);
    valueLabel->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: 700; color: %1;").arg(color));
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    return std::make_tuple(card, valueLabel);
  };

  auto [totalCard, totalVal] = makeCard(QStringLiteral("总检测数"), QStringLiteral("#f8fafc"));
  auto [okCard, okVal] = makeCard(QStringLiteral("OK 数量"), QStringLiteral("#22c55e"));
  auto [ngCard, ngVal] = makeCard(QStringLiteral("NG 数量"), QStringLiteral("#ef4444"));
  auto [yieldCard, yieldVal] = makeCard(QStringLiteral("良率"), QStringLiteral("#3b82f6"));

  totalCountLabel_ = totalVal;
  okCountLabel_ = okVal;
  ngCountLabel_ = ngVal;
  yieldLabel_ = yieldVal;

  summaryLayout->addWidget(totalCard);
  summaryLayout->addWidget(okCard);
  summaryLayout->addWidget(ngCard);
  summaryLayout->addWidget(yieldCard);
  rootLayout->addWidget(summaryGroup);

  // ── Filter bar ──
  auto *filterLayout = new QHBoxLayout;
  filterLayout->setSpacing(10);

  auto *filterLabel = new QLabel(QStringLiteral("筛选:"), this);
  filterLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-weight: 600;"));
  filterLayout->addWidget(filterLabel);

  decisionFilterComboBox_ = new QComboBox(this);
  decisionFilterComboBox_->addItem(QStringLiteral("全部结果"), QString());
  decisionFilterComboBox_->addItem(QStringLiteral("仅 OK"), QStringLiteral("OK"));
  decisionFilterComboBox_->addItem(QStringLiteral("仅 NG"), QStringLiteral("NG"));
  decisionFilterComboBox_->setStyleSheet(QStringLiteral(
      "QComboBox { background: #0f172a; color: #e2e8f0; border: 1px solid #334155; "
      "border-radius: 6px; padding: 4px 10px; }"));
  filterLayout->addWidget(decisionFilterComboBox_);

  programFilterEdit_ = new QLineEdit(this);
  programFilterEdit_->setPlaceholderText(QStringLiteral("按程序名筛选…"));
  programFilterEdit_->setStyleSheet(QStringLiteral(
      "QLineEdit { background: #0f172a; color: #e2e8f0; border: 1px solid #334155; "
      "border-radius: 6px; padding: 4px 10px; }"));
  filterLayout->addWidget(programFilterEdit_);

  auto *applyFilterButton = new QPushButton(QStringLiteral("应用筛选"), this);
  applyFilterButton->setStyleSheet(QStringLiteral(
      "QPushButton { background: #1e3a5f; color: #e2e8f0; border: 1px solid #334155; "
      "border-radius: 6px; padding: 6px 14px; font-weight: 600; }"
      "QPushButton:hover { background: #2563eb; }"));
  connect(applyFilterButton, &QPushButton::clicked, this, &ProductionHistoryDialog::applyFilter);
  filterLayout->addWidget(applyFilterButton);

  auto *refreshButton = new QPushButton(QStringLiteral("刷新"), this);
  refreshButton->setStyleSheet(QStringLiteral(
      "QPushButton { background: #1e3a5f; color: #e2e8f0; border: 1px solid #334155; "
      "border-radius: 6px; padding: 6px 14px; font-weight: 600; }"
      "QPushButton:hover { background: #2563eb; }"));
  connect(refreshButton, &QPushButton::clicked, this, &ProductionHistoryDialog::refreshData);
  filterLayout->addWidget(refreshButton);

  filterLayout->addStretch();
  rootLayout->addLayout(filterLayout);

  // ── Results table ──
  resultTable_ = new QTableWidget(this);
  resultTable_->setColumnCount(7);
  resultTable_->setHorizontalHeaderLabels({
      QStringLiteral("板ID"),
      QStringLiteral("时间"),
      QStringLiteral("程序"),
      QStringLiteral("判定"),
      QStringLiteral("AI标签"),
      QStringLiteral("Mark数"),
      QStringLiteral("ROI数"),
  });
  resultTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  resultTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  resultTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  resultTable_->setAlternatingRowColors(true);
  resultTable_->verticalHeader()->setVisible(false);
  resultTable_->horizontalHeader()->setStretchLastSection(true);
  resultTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  resultTable_->setStyleSheet(QStringLiteral(
      "QTableWidget { background: #0f172a; color: #e2e8f0; border: 1px solid #334155; "
      "border-radius: 8px; gridline-color: #1e293b; font-size: 12px; }"
      "QTableWidget::item { padding: 6px 10px; }"
      "QTableWidget::item:selected { background: #1e3a5f; }"
      "QHeaderView::section { background: #1e293b; color: #94a3b8; padding: 6px 10px; "
      "border: none; border-bottom: 1px solid #334155; font-weight: 600; }"));
  connect(resultTable_, &QTableWidget::cellDoubleClicked, this, &ProductionHistoryDialog::showDetail);
  rootLayout->addWidget(resultTable_, 1);

  // ── Detail panel ──
  auto *detailGroup = new QGroupBox(QStringLiteral("检测详情（双击表格行查看）"), this);
  detailGroup->setStyleSheet(QStringLiteral(
      "QGroupBox { color: #e2e8f0; font-weight: 600; border: 1px solid #334155; "
      "border-radius: 8px; margin-top: 10px; padding-top: 14px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 12px; }"));
  auto *detailLayout = new QVBoxLayout(detailGroup);
  detailLayout->setContentsMargins(10, 16, 10, 10);

  detailTextEdit_ = new QTextEdit(this);
  detailTextEdit_->setReadOnly(true);
  detailTextEdit_->setPlaceholderText(QStringLiteral("双击上方的检测记录行以查看详情…"));
  detailTextEdit_->setMaximumHeight(160);
  detailTextEdit_->setStyleSheet(QStringLiteral(
      "QTextEdit { background: #020617; color: #e2e8f0; border: 1px solid #334155; "
      "border-radius: 6px; font-family: 'SF Mono', 'Menlo', monospace; font-size: 11px; }"));
  detailLayout->addWidget(detailTextEdit_);
  rootLayout->addWidget(detailGroup);

  // ── Button box ──
  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
  buttonBox->button(QDialogButtonBox::Close)->setStyleSheet(QStringLiteral(
      "QPushButton { background: #334155; color: #f8fafc; padding: 8px 18px; "
      "border-radius: 8px; font-weight: 600; }"));
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::close);
  rootLayout->addWidget(buttonBox);
}

void ProductionHistoryDialog::refreshData() {
  if (databaseManager_ == nullptr || !databaseManager_->isOpen()) {
    return;
  }

  const auto result = databaseManager_->queryInspectionResults(500);
  if (!result) {
    return;
  }

  currentRecords_ = QVector<InspectionRecord>(result.value.begin(), result.value.end());
  applyFilter();
}

void ProductionHistoryDialog::applyFilter() {
  const QString decisionFilter = decisionFilterComboBox_->currentData().toString();
  const QString programFilter = programFilterEdit_->text().trimmed();

  int totalOk = 0;
  int totalNg = 0;

  resultTable_->setRowCount(0);

  for (const auto &record : currentRecords_) {
    if (!decisionFilter.isEmpty() &&
        QString::fromStdString(record.finalDecision) != decisionFilter) {
      continue;
    }
    if (!programFilter.isEmpty() &&
        !QString::fromStdString(record.programName).contains(programFilter, Qt::CaseInsensitive)) {
      continue;
    }

    const int row = resultTable_->rowCount();
    resultTable_->insertRow(row);

    resultTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(record.boardId)));
    resultTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(record.timestamp)));
    resultTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(record.programName)));

    auto *decisionItem = new QTableWidgetItem(QString::fromStdString(record.finalDecision));
    decisionItem->setForeground(
        record.finalDecision == "OK" ? QColor("#22c55e") : QColor("#ef4444"));
    resultTable_->setItem(row, 3, decisionItem);

    resultTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(record.aiLabel)));
    resultTable_->setItem(row, 5,
                          new QTableWidgetItem(QString::number(record.marksCount)));
    resultTable_->setItem(row, 6,
                          new QTableWidgetItem(QString::number(record.roisCount)));

    if (record.finalDecision == "OK") {
      ++totalOk;
    } else {
      ++totalNg;
    }
  }

  const int total = totalOk + totalNg;
  totalCountLabel_->setText(QString::number(total));
  okCountLabel_->setText(QString::number(totalOk));
  ngCountLabel_->setText(QString::number(totalNg));

  if (total > 0) {
    const double yield = 100.0 * static_cast<double>(totalOk) / static_cast<double>(total);
    yieldLabel_->setText(QStringLiteral("%1%").arg(yield, 0, 'f', 1));
  } else {
    yieldLabel_->setText(QStringLiteral("—"));
  }

  resultTable_->resizeColumnsToContents();
}

void ProductionHistoryDialog::showDetail(const int row) {
  if (row < 0 || row >= resultTable_->rowCount()) {
    return;
  }

  // Map table row back to the filtered record.
  const auto *boardItem = resultTable_->item(row, 0);
  if (boardItem == nullptr) {
    return;
  }

  const std::string boardId = boardItem->text().toStdString();
  for (const auto &record : currentRecords_) {
    if (record.boardId == boardId) {
      QString detail;
      detail += QStringLiteral("板ID: %1\n").arg(QString::fromStdString(record.boardId));
      detail += QStringLiteral("时间: %1\n").arg(QString::fromStdString(record.timestamp));
      detail += QStringLiteral("程序: %1\n").arg(QString::fromStdString(record.programName));
      detail += QStringLiteral("判定: %1\n").arg(QString::fromStdString(record.finalDecision));
      detail += QStringLiteral("AI标签: %1 (置信度 %2%)\n")
                    .arg(QString::fromStdString(record.aiLabel))
                    .arg(record.aiConfidence * 100.0, 0, 'f', 1);
      detail += QStringLiteral("Mark数: %1  ROI数: %2\n")
                    .arg(record.marksCount)
                    .arg(record.roisCount);
      detail += QStringLiteral("\n详情JSON:\n%1")
                    .arg(QString::fromStdString(record.detailsJson));
      detailTextEdit_->setPlainText(detail);
      return;
    }
  }
}

#endif
