#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "calibration/CalibrationTypes.h"
#include "calibration/OriginCalibrationModule.h"
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

/// 原点校正窗口
///
/// 左右布局：
///   左侧 — 实时虚拟相机 FOV 画面（带十字准星）
///   右侧 — 点动控制面板 + 原点专属动作按钮
class OriginCalibDialog final : public QDialog {
  Q_OBJECT

public:
  using FrameProvider = std::function<QImage()>;
  using JogProvider = std::function<void(double dx, double dy)>;
  using PoseProvider = std::function<MechanicalPose()>;
  using PoseInfoProvider = std::function<VirtualCameraPoseInfo()>;

  explicit OriginCalibDialog(QWidget *parent = nullptr);

  void setFrameProvider(FrameProvider frameProvider);
  void setJogProvider(JogProvider jogProvider);
  void setPoseProvider(PoseProvider poseProvider);
  void setPoseInfoProvider(PoseInfoProvider poseInfoProvider);
  void setProgramContext(const ProgramModel &program);

signals:
  /// 用户点击"应用原点"时发出
  /// 参数：换算后的逻辑原点机械坐标
  void originApplied(double originX, double originY, double originZ, double originR);

  /// 用户点动相机时发出，供外部同步机械坐标刷新
  void cameraJogged(double dx, double dy);

private:
  void onSetOriginReference();
  void onApplyOrigin();
  void onReset();
  void onJog(double dx, double dy);
  void updateResultDisplay();
  void updateCoordinateDisplay();
  void maybeReject();
  void showEvent(QShowEvent *event) override;

  CameraPreviewWidget *cameraPreview_ {nullptr};
  CalibrationJogPanel *jogPanel_ {nullptr};

  QLabel *resultSummaryLabel_ {nullptr};
  QPushButton *setReferenceButton_ {nullptr};
  QPushButton *applyOriginButton_ {nullptr};
  QPushButton *resetButton_ {nullptr};

  // External providers
  FrameProvider frameProvider_;
  JogProvider jogProvider_;
  PoseProvider poseProvider_;
  PoseInfoProvider poseInfoProvider_;

  // Program context
  BoardDefinition boardDefinition_;
  OriginCalibration originCalibration_;

  // Temporary state
  calibration::OriginCalibrationModule originModule_;
  bool hasUnappliedChanges_ {false};
};

#endif
