#include "ui/OriginCalibDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QLabel>
#include <QVBoxLayout>

OriginCalibDialog::OriginCalibDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Origin Calibration"));
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(QStringLiteral("Mechanical origin alignment placeholder"), this));
}

#endif

