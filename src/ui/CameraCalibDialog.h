#pragma once

#ifdef AOI_HAS_QT_WIDGETS
#include <QDialog>

class CameraCalibDialog final : public QDialog {
  Q_OBJECT

public:
  explicit CameraCalibDialog(QWidget *parent = nullptr);
};
#endif

