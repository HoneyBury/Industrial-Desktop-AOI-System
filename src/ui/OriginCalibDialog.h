#pragma once

#ifdef AOI_HAS_QT_WIDGETS
#include <QDialog>

#include "calibration/CalibrationTypes.h"
#include "vision/CameraCalibrator.h"

#include <QImage>

#include <functional>

class CalibrationCaptureWidget;
class QLabel;
class QPushButton;

class OriginCalibDialog final : public QDialog {
  Q_OBJECT

public:
  explicit OriginCalibDialog(QWidget *parent = nullptr);

  void setFrameProvider(std::function<QImage()> frameProvider,
                        std::function<bool()> runningProvider);
  void setCalibrationContext(const CameraCalibrationData &calibrationData,
                             const MechanicalPose &currentPose);
  [[nodiscard]] QImage capturedFrame() const;

signals:
  void correctedPoseApplied(double x, double y, double z, double r);

private:
  CameraCalibrationData calibrationData_ {};
  PixelScaleCalibration pixelScaleCalibration_ {};
  MechanicalPose currentPose_ {};
  CalibrationCaptureWidget *captureWidget_ {nullptr};
  QLabel *resultSummaryLabel_ {nullptr};
  QPushButton *applyPoseButton_ {nullptr};
  MechanicalPose correctedPose_ {};
  bool hasCorrectedPose_ {false};
};
#endif
