#include "ui/MarkEditDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

MarkEditDialog::MarkEditDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("添加 Mark 点"));
  resize(360, 220);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->addWidget(new QLabel(QStringLiteral("配置当前程序的 Mark 点坐标与置信度。"), this));

  auto *formLayout = new QFormLayout;
  xSpinBox_ = new QDoubleSpinBox(this);
  xSpinBox_->setRange(-9999.0, 9999.0);
  xSpinBox_->setDecimals(3);
  xSpinBox_->setValue(100.0);
  ySpinBox_ = new QDoubleSpinBox(this);
  ySpinBox_->setRange(-9999.0, 9999.0);
  ySpinBox_->setDecimals(3);
  ySpinBox_->setValue(80.0);
  scoreSpinBox_ = new QDoubleSpinBox(this);
  scoreSpinBox_->setRange(0.0, 1.0);
  scoreSpinBox_->setDecimals(3);
  scoreSpinBox_->setValue(1.0);
  formLayout->addRow(QStringLiteral("X"), xSpinBox_);
  formLayout->addRow(QStringLiteral("Y"), ySpinBox_);
  formLayout->addRow(QStringLiteral("Score"), scoreSpinBox_);
  rootLayout->addLayout(formLayout);

  auto *buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  rootLayout->addWidget(buttonBox);
}

MarkPoint MarkEditDialog::markPoint() const {
  MarkPoint mark;
  mark.x = xSpinBox_->value();
  mark.y = ySpinBox_->value();
  mark.score = scoreSpinBox_->value();
  mark.previewScore = scoreSpinBox_->value();
  mark.minimumScore = 0.8;
  return mark;
}

#endif
