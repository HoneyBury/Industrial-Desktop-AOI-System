#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "motion/VirtualMotionController.h"

#include <QDialog>
#include <QString>

class AnimationWidget;
class VirtualTransportController;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QTimer;

/// 运动控制与虚拟设备调试窗口
///
/// 集成设备动画、IO 控制、轴控制面板、运控日志，是虚拟运控的一站式调试界面。
class MotionControlDialog final : public QDialog {
  Q_OBJECT

public:
  explicit MotionControlDialog(VirtualMotionController *motion,
                               VirtualTransportController *transport,
                               QWidget *parent = nullptr);

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

  // IO 操作
  void raiseStopper();
  void lowerStopper();
  void loadBoard();
  void unloadBoard();
  void resetBoard();

  [[nodiscard]] QString axisName(MotionAxis axis) const;
  [[nodiscard]] QDoubleSpinBox *targetSpinBox(MotionAxis axis) const;
  [[nodiscard]] QDoubleSpinBox *stepSpinBox(MotionAxis axis) const;
  [[nodiscard]] QLabel *positionLabel(MotionAxis axis) const;
  [[nodiscard]] QLabel *stateLabel(MotionAxis axis) const;

  VirtualMotionController *motion_ {nullptr};
  VirtualTransportController *transport_ {nullptr};

  QScrollArea *scrollArea_ {nullptr};

  // 动画
  AnimationWidget *animationWidget_ {nullptr};
  QTimer *refreshTimer_ {nullptr};

  // 轴面板
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

  // IO 状态标签
  QLabel *boardStateValueLabel_ {nullptr};
  QLabel *boardPosValueLabel_ {nullptr};
  QLabel *stopperStateValueLabel_ {nullptr};
  QLabel *conveyorSpeedValueLabel_ {nullptr};

  // 缩放
  QLabel *zoomValueLabel_ {nullptr};

  // 按钮
  QPushButton *emergencyStopButton_ {nullptr};
  QPushButton *resetStopButton_ {nullptr};
  QTextEdit *logTextEdit_ {nullptr};
};

#endif
