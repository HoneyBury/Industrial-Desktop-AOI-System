#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "calibration/CalibrationTypes.h"

#include <QDialog>
#include <QImage>

#include <functional>

class CalibrationCaptureWidget;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

class LaserOffsetCalibDialog final : public QDialog {
  Q_OBJECT

public:
  explicit LaserOffsetCalibDialog(QWidget *parent = nullptr);

  void setFrameProvider(std::function<QImage()> frameProvider,
                        std::function<bool()> runningProvider);
  void setCalibrationContext(const PixelScaleCalibration &pixelScale,
                             const PixelPoint &opticalCenter);

signals:
  void calibrationApplied(const LaserOffsetCalibration &calibration);

private:
  void computeOffset();
  void updateApplyButtonState();

  CalibrationCaptureWidget *captureWidget_ {nullptr};
  QDoubleSpinBox *laserCrossXSpinBox_ {nullptr};
  QDoubleSpinBox *laserCrossYSpinBox_ {nullptr};
  QLabel *opticalCenterValueLabel_ {nullptr};
  QLabel *resultSummaryLabel_ {nullptr};
  QPushButton *applyButton_ {nullptr};
  PixelScaleCalibration pixelScale_ {};
  PixelPoint opticalCenter_ {};
  LaserOffsetCalibration computedCalibration_ {};
  bool hasComputedCalibration_ {false};
};

#endif
