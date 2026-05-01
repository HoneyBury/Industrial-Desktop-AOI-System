#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "calibration/CalibrationTypes.h"
#include "calibration/LaserOffsetCalibrationModule.h"
#include "camera/VirtualCameraDevice.h"
#include "program/ProgramModel.h"

#include <QDialog>
#include <QGroupBox>
#include <QImage>
#include <QShowEvent>

#include <functional>

class CalibrationJogPanel;
class CameraPreviewWidget;
class QLabel;
class QPushButton;

/// 镭射点偏移校正窗口
///
/// 左右布局（与原点校正窗口相同）：
///   左侧 — 实时虚拟相机 FOV 画面（带十字准星）
///   右侧 — 点动控制面板 + 偏移校正专属动作按钮
class LaserOffsetCalibDialog final : public QDialog {
  Q_OBJECT

public:
  using FrameProvider = std::function<QImage()>;
  using JogProvider = std::function<void(double dx, double dy)>;
  using PoseProvider = std::function<MechanicalPose()>;
  using PoseInfoProvider = std::function<VirtualCameraPoseInfo()>;

  explicit LaserOffsetCalibDialog(QWidget *parent = nullptr);

  void setFrameProvider(FrameProvider frameProvider);
  void setJogProvider(JogProvider jogProvider);
  void setPoseProvider(PoseProvider poseProvider);
  void setPoseInfoProvider(PoseInfoProvider poseInfoProvider);
  void setProgramContext(const ProgramModel &program);

signals:
  void calibrationApplied(const LaserOffsetCalibration &calibration);
  void cameraJogged(double dx, double dy);

private:
  void onRecordCameraPoint();
  void onRecordLaserPoint();
  void onComputeOffset();
  void onApplyOffset();
  void onClearRecords();
  void onJog(double dx, double dy);
  void updateResultDisplay();
  void maybeReject();
  void showEvent(QShowEvent *event) override;

  CameraPreviewWidget *cameraPreview_ {nullptr};
  CalibrationJogPanel *jogPanel_ {nullptr};

  QLabel *resultSummaryLabel_ {nullptr};
  QPushButton *recordCameraBtn_ {nullptr};
  QPushButton *recordLaserBtn_ {nullptr};
  QPushButton *computeOffsetBtn_ {nullptr};
  QPushButton *applyOffsetBtn_ {nullptr};
  QPushButton *clearBtn_ {nullptr};

  // Providers
  FrameProvider frameProvider_;
  JogProvider jogProvider_;
  PoseProvider poseProvider_;
  PoseInfoProvider poseInfoProvider_;

  // Program context
  PixelScaleCalibration pixelScale_;

  // Temporary state
  calibration::LaserOffsetCalibrationModule laserModule_;
  bool hasUnappliedResult_ {false};
};

#endif
