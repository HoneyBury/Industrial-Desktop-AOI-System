#include "ui/MarkEditDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

MarkEditDialog::MarkEditDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Mark Point Editor"));
  resize(400, 400);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->addWidget(new QLabel(QStringLiteral("Configure mark point properties."), this));

  auto *formLayout = new QFormLayout;

  nameEdit_ = new QLineEdit(this);
  nameEdit_->setPlaceholderText(QStringLiteral("Mark-1"));
  formLayout->addRow(QStringLiteral("Name"), nameEdit_);

  xSpinBox_ = new QDoubleSpinBox(this);
  xSpinBox_->setRange(-9999.0, 9999.0);
  xSpinBox_->setDecimals(3);
  xSpinBox_->setValue(100.0);
  formLayout->addRow(QStringLiteral("X"), xSpinBox_);

  ySpinBox_ = new QDoubleSpinBox(this);
  ySpinBox_->setRange(-9999.0, 9999.0);
  ySpinBox_->setDecimals(3);
  ySpinBox_->setValue(80.0);
  formLayout->addRow(QStringLiteral("Y"), ySpinBox_);

  widthSpinBox_ = new QDoubleSpinBox(this);
  widthSpinBox_->setRange(1.0, 9999.0);
  widthSpinBox_->setDecimals(1);
  widthSpinBox_->setValue(48.0);
  formLayout->addRow(QStringLiteral("Width"), widthSpinBox_);

  heightSpinBox_ = new QDoubleSpinBox(this);
  heightSpinBox_->setRange(1.0, 9999.0);
  heightSpinBox_->setDecimals(1);
  heightSpinBox_->setValue(48.0);
  formLayout->addRow(QStringLiteral("Height"), heightSpinBox_);

  rotationSpinBox_ = new QDoubleSpinBox(this);
  rotationSpinBox_->setRange(-180.0, 180.0);
  rotationSpinBox_->setDecimals(2);
  rotationSpinBox_->setValue(0.0);
  formLayout->addRow(QStringLiteral("Rotation"), rotationSpinBox_);

  scoreSpinBox_ = new QDoubleSpinBox(this);
  scoreSpinBox_->setRange(0.0, 1.0);
  scoreSpinBox_->setDecimals(3);
  scoreSpinBox_->setValue(0.9);
  formLayout->addRow(QStringLiteral("Score"), scoreSpinBox_);

  minScoreSpinBox_ = new QDoubleSpinBox(this);
  minScoreSpinBox_->setRange(0.0, 1.0);
  minScoreSpinBox_->setDecimals(3);
  minScoreSpinBox_->setValue(0.8);
  formLayout->addRow(QStringLiteral("Min Score"), minScoreSpinBox_);

  shapeComboBox_ = new QComboBox(this);
  shapeComboBox_->addItems({
      QString::fromStdString(std::string(toString(MarkShape::Rectangle))),
      QString::fromStdString(std::string(toString(MarkShape::Circle))),
      QString::fromStdString(std::string(toString(MarkShape::Diamond))),
      QString::fromStdString(std::string(toString(MarkShape::Cross))),
  });
  formLayout->addRow(QStringLiteral("Shape"), shapeComboBox_);

  algorithmComboBox_ = new QComboBox(this);
  algorithmComboBox_->addItems({
      QString::fromStdString(std::string(toString(MarkAlgorithm::ColorBrushTemplate))),
      QString::fromStdString(std::string(toString(MarkAlgorithm::BinaryGeometry))),
  });
  formLayout->addRow(QStringLiteral("Algorithm"), algorithmComboBox_);

  rootLayout->addLayout(formLayout);

  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  rootLayout->addWidget(buttonBox);
}

void MarkEditDialog::setMarkPoint(const MarkPoint &mark) {
  nameEdit_->setText(QString::fromStdString(mark.name));
  xSpinBox_->setValue(mark.x);
  ySpinBox_->setValue(mark.y);
  widthSpinBox_->setValue(mark.width);
  heightSpinBox_->setValue(mark.height);
  rotationSpinBox_->setValue(mark.rotation);
  scoreSpinBox_->setValue(mark.score);
  minScoreSpinBox_->setValue(mark.minimumScore);
  const int shapeIdx = shapeComboBox_->findText(
      QString::fromStdString(std::string(toString(mark.shape))));
  if (shapeIdx >= 0) shapeComboBox_->setCurrentIndex(shapeIdx);
  const int algoIdx = algorithmComboBox_->findText(
      QString::fromStdString(std::string(toString(mark.algorithm))));
  if (algoIdx >= 0) algorithmComboBox_->setCurrentIndex(algoIdx);
}

MarkPoint MarkEditDialog::markPoint() const {
  MarkPoint mark;
  mark.name = nameEdit_->text().isEmpty() ? "Mark-New" : nameEdit_->text().toStdString();
  mark.x = xSpinBox_->value();
  mark.y = ySpinBox_->value();
  mark.width = widthSpinBox_->value();
  mark.height = heightSpinBox_->value();
  mark.rotation = rotationSpinBox_->value();
  mark.score = scoreSpinBox_->value();
  mark.previewScore = scoreSpinBox_->value();
  mark.minimumScore = minScoreSpinBox_->value();
  mark.shape = markShapeFromString(shapeComboBox_->currentText().toStdString());
  mark.algorithm = markAlgorithmFromString(algorithmComboBox_->currentText().toStdString());
  mark.enabled = true;
  return mark;
}

#endif
