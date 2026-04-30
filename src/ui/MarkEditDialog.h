#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "vision/MarkDetector.h"

#include <QDialog>

class QDoubleSpinBox;

class MarkEditDialog final : public QDialog {
  Q_OBJECT

public:
  explicit MarkEditDialog(QWidget *parent = nullptr);

  [[nodiscard]] MarkPoint markPoint() const;

private:
  QDoubleSpinBox *xSpinBox_ {nullptr};
  QDoubleSpinBox *ySpinBox_ {nullptr};
  QDoubleSpinBox *scoreSpinBox_ {nullptr};
};

#endif
