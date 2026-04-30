#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "motion/VirtualMotionController.h"

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class QDoubleSpinBox;
class QTextEdit;

class MotionControlDialog final : public QDialog {
  Q_OBJECT

public:
  explicit MotionControlDialog(VirtualMotionController *controller, QWidget *parent = nullptr);

signals:
  void motionStateChanged();
  void motionLogGenerated(const QString &message);

private:
  void buildUi();
  void refreshUi();
  void appendLog(const QString &message);
  void moveAbsolute(MotionAxis axis);
  void jog(MotionAxis axis, double direction);
  void home(MotionAxis axis);

  [[nodiscard]] QString axisName(MotionAxis axis) const;
  [[nodiscard]] QDoubleSpinBox *targetSpinBox(MotionAxis axis) const;
  [[nodiscard]] QDoubleSpinBox *stepSpinBox(MotionAxis axis) const;
  [[nodiscard]] QLabel *positionLabel(MotionAxis axis) const;
  [[nodiscard]] QLabel *stateLabel(MotionAxis axis) const;

  VirtualMotionController *controller_ {nullptr};
  QLabel *xPositionValueLabel_ {nullptr};
  QLabel *yPositionValueLabel_ {nullptr};
  QLabel *zPositionValueLabel_ {nullptr};
  QLabel *rPositionValueLabel_ {nullptr};
  QLabel *xStateValueLabel_ {nullptr};
  QLabel *yStateValueLabel_ {nullptr};
  QLabel *zStateValueLabel_ {nullptr};
  QLabel *rStateValueLabel_ {nullptr};
  QDoubleSpinBox *xTargetSpinBox_ {nullptr};
  QDoubleSpinBox *yTargetSpinBox_ {nullptr};
  QDoubleSpinBox *zTargetSpinBox_ {nullptr};
  QDoubleSpinBox *rTargetSpinBox_ {nullptr};
  QDoubleSpinBox *xStepSpinBox_ {nullptr};
  QDoubleSpinBox *yStepSpinBox_ {nullptr};
  QDoubleSpinBox *zStepSpinBox_ {nullptr};
  QDoubleSpinBox *rStepSpinBox_ {nullptr};
  QPushButton *emergencyStopButton_ {nullptr};
  QPushButton *resetStopButton_ {nullptr};
  QTextEdit *logTextEdit_ {nullptr};
};

#endif

