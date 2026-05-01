#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "coordinate/CoordinateTransformer.h"

#include <QWidget>

#include <functional>

class QDoubleSpinBox;
class QLabel;
class QPushButton;

/// 校正窗口通用点动控制面板
/// 包含：步长预设/输入、方向点动按钮、机械坐标显示、像素坐标显示
class CalibrationJogPanel final : public QWidget {
  Q_OBJECT

public:
  using JogCallback = std::function<void(double dx, double dy)>;

  explicit CalibrationJogPanel(QWidget *parent = nullptr);

  void setJogCallback(JogCallback callback);
  void setCurrentMechanicalPose(const MechanicalPose &pose);
  void setCurrentPixelCenter(double px, double py);
  void setStepSize(double mm);
  [[nodiscard]] double stepSize() const;

signals:
  void stepSizeChanged(double mm);
  void jogged(double dx, double dy);

private:
  void onPresetStep(double mm);
  void onJogUp();
  void onJogDown();
  void onJogLeft();
  void onJogRight();
  void updateCoordinateDisplay();

  JogCallback jogCallback_;

  QDoubleSpinBox *stepSizeSpinBox_ {nullptr};
  QPushButton *step01Button_ {nullptr};
  QPushButton *step1Button_ {nullptr};
  QPushButton *step1_0Button_ {nullptr};
  QPushButton *step5_0Button_ {nullptr};

  QPushButton *jogUpButton_ {nullptr};
  QPushButton *jogDownButton_ {nullptr};
  QPushButton *jogLeftButton_ {nullptr};
  QPushButton *jogRightButton_ {nullptr};

  QLabel *machineXLabel_ {nullptr};
  QLabel *machineYLabel_ {nullptr};
  QLabel *machineZLabel_ {nullptr};
  QLabel *machineRLabel_ {nullptr};
  QLabel *pixelPxLabel_ {nullptr};
  QLabel *pixelPyLabel_ {nullptr};

  MechanicalPose currentPose_;
  double currentPx_ {0.0};
  double currentPy_ {0.0};
  double currentStep_ {1.0};
};

#endif
