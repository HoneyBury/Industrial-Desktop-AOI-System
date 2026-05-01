#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "vision/MarkDetector.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;

class MarkEditDialog final : public QDialog {
  Q_OBJECT

public:
  explicit MarkEditDialog(QWidget *parent = nullptr);

  void setMarkPoint(const MarkPoint &mark);
  [[nodiscard]] MarkPoint markPoint() const;

private:
  QLineEdit *nameEdit_ {nullptr};
  QDoubleSpinBox *xSpinBox_ {nullptr};
  QDoubleSpinBox *ySpinBox_ {nullptr};
  QDoubleSpinBox *widthSpinBox_ {nullptr};
  QDoubleSpinBox *heightSpinBox_ {nullptr};
  QDoubleSpinBox *rotationSpinBox_ {nullptr};
  QDoubleSpinBox *scoreSpinBox_ {nullptr};
  QDoubleSpinBox *minScoreSpinBox_ {nullptr};
  QComboBox *shapeComboBox_ {nullptr};
  QComboBox *algorithmComboBox_ {nullptr};
};

#endif
