#include "ui/NewProgramDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QDoubleSpinBox *createDimensionSpinBox(QWidget *parent, const double minimum, const double maximum,
                                       const double value, const QString &suffix) {
  auto *spinBox = new QDoubleSpinBox(parent);
  spinBox->setRange(minimum, maximum);
  spinBox->setDecimals(2);
  spinBox->setValue(value);
  spinBox->setSuffix(suffix);
  spinBox->setSingleStep(1.0);
  return spinBox;
}

} // namespace

NewProgramDialog::NewProgramDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("新建整板程序"));
  resize(520, 420);

  auto *rootLayout = new QVBoxLayout(this);

  auto *introLabel = new QLabel(
      QStringLiteral("请先定义板尺寸、轨道宽度和扫描 FOV。创建后工作台会按这些参数生成整板预览。"), this);
  introLabel->setWordWrap(true);
  rootLayout->addWidget(introLabel);

  auto *programGroupBox = new QGroupBox(QStringLiteral("程序信息"), this);
  auto *programFormLayout = new QFormLayout(programGroupBox);
  programNameLineEdit_ = new QLineEdit(QStringLiteral("board_program"), programGroupBox);
  programFormLayout->addRow(QStringLiteral("程序名称"), programNameLineEdit_);
  rootLayout->addWidget(programGroupBox);

  auto *boardGroupBox = new QGroupBox(QStringLiteral("板参数"), this);
  auto *boardFormLayout = new QFormLayout(boardGroupBox);
  boardLengthSpinBox_ = createDimensionSpinBox(boardGroupBox, 10.0, 2000.0, 260.0, QStringLiteral(" mm"));
  boardWidthSpinBox_ = createDimensionSpinBox(boardGroupBox, 10.0, 2000.0, 180.0, QStringLiteral(" mm"));
  railWidthSpinBox_ = createDimensionSpinBox(boardGroupBox, 5.0, 300.0, 32.0, QStringLiteral(" mm"));
  boardFormLayout->addRow(QStringLiteral("板长"), boardLengthSpinBox_);
  boardFormLayout->addRow(QStringLiteral("板宽"), boardWidthSpinBox_);
  boardFormLayout->addRow(QStringLiteral("轨道宽度"), railWidthSpinBox_);
  rootLayout->addWidget(boardGroupBox);

  auto *scanGroupBox = new QGroupBox(QStringLiteral("扫描参数"), this);
  auto *scanFormLayout = new QFormLayout(scanGroupBox);
  fovWidthSpinBox_ = createDimensionSpinBox(scanGroupBox, 1.0, 500.0, 32.0, QStringLiteral(" mm"));
  fovHeightSpinBox_ = createDimensionSpinBox(scanGroupBox, 1.0, 500.0, 24.0, QStringLiteral(" mm"));
  scanOrderComboBox_ = new QComboBox(scanGroupBox);
  scanOrderComboBox_->addItem(QStringLiteral("从左到右"), static_cast<int>(ScanOrder::LeftToRight));
  scanOrderComboBox_->addItem(QStringLiteral("从上到下"), static_cast<int>(ScanOrder::TopToBottom));
  scanFormLayout->addRow(QStringLiteral("FOV 宽度"), fovWidthSpinBox_);
  scanFormLayout->addRow(QStringLiteral("FOV 高度"), fovHeightSpinBox_);
  scanFormLayout->addRow(QStringLiteral("扫描顺序"), scanOrderComboBox_);
  rootLayout->addWidget(scanGroupBox);

  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  rootLayout->addWidget(buttonBox);
}

void NewProgramDialog::setInitialProgramName(const QString &programName) { programNameLineEdit_->setText(programName); }

void NewProgramDialog::setInitialBoardDefinition(const BoardDefinition &boardDefinition) {
  boardLengthSpinBox_->setValue(boardDefinition.boardLengthMm);
  boardWidthSpinBox_->setValue(boardDefinition.boardWidthMm);
  railWidthSpinBox_->setValue(boardDefinition.railWidthMm);
}

void NewProgramDialog::setInitialScanRecipe(const ScanRecipe &scanRecipe) {
  fovWidthSpinBox_->setValue(scanRecipe.fovWidthMm);
  fovHeightSpinBox_->setValue(scanRecipe.fovHeightMm);
  const int index = scanRecipe.scanOrder == ScanOrder::TopToBottom ? 1 : 0;
  scanOrderComboBox_->setCurrentIndex(index);
}

QString NewProgramDialog::programName() const { return programNameLineEdit_->text().trimmed(); }

BoardDefinition NewProgramDialog::boardDefinition() const {
  return BoardDefinition {boardLengthSpinBox_->value(), boardWidthSpinBox_->value(), railWidthSpinBox_->value()};
}

ScanRecipe NewProgramDialog::scanRecipe() const {
  return ScanRecipe {
      fovWidthSpinBox_->value(),
      fovHeightSpinBox_->value(),
      scanOrderComboBox_->currentData().toInt() == static_cast<int>(ScanOrder::TopToBottom)
          ? ScanOrder::TopToBottom
          : ScanOrder::LeftToRight,
      true,
  };
}

#endif
