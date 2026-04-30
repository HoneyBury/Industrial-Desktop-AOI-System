#include "ui/CameraCalibDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QVBoxLayout>
#include <QLabel>

CameraCalibDialog::CameraCalibDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Camera Calibration"));
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(QStringLiteral("Chessboard calibration workflow placeholder"), this));
}

#endif

