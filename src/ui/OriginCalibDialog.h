#pragma once

#ifdef AOI_HAS_QT_WIDGETS
#include <QDialog>

class OriginCalibDialog final : public QDialog {
  Q_OBJECT

public:
  explicit OriginCalibDialog(QWidget *parent = nullptr);
};
#endif

