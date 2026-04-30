#pragma once

#include "camera/UsbCamera.h"
#include "motion/VirtualMotionController.h"
#include "program/ProgramManager.h"

#ifdef AOI_HAS_QT_WIDGETS
#include <QMainWindow>
#include <QString>

class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

private:
  void bindCameraControls();
  void bindMotionControls();
  void bindProgramControls();
  void refreshCameraPanel();
  void refreshStatus();
  void refreshMotionPanel();
  void refreshProgramSummary();
  void startCameraPreview();
  void stopCameraPreview();
  void updateCameraPreview();
  void appendLog(const QString &message);
  void moveAxisAbsolute(MotionAxis axis);
  void moveAxisRelative(MotionAxis axis, double direction);
  void homeAxis(MotionAxis axis);
  void emergencyStopMotion();
  void resetEmergencyStopMotion();
  void createDefaultProgram();
  void loadDefaultProgram();
  void saveCurrentProgram();

  [[nodiscard]] QString axisName(MotionAxis axis) const;
  [[nodiscard]] QString cameraModeText() const;
  [[nodiscard]] QString projectRootPath() const;
  [[nodiscard]] QString projectFilePath(const QString &relativePath) const;
  [[nodiscard]] QDoubleSpinBox *targetSpinBox(MotionAxis axis) const;
  [[nodiscard]] QDoubleSpinBox *stepSpinBox(MotionAxis axis) const;
  [[nodiscard]] QLabel *positionLabel(MotionAxis axis) const;
  [[nodiscard]] QLabel *axisStateLabel(MotionAxis axis) const;

  Ui::MainWindow *ui_;
  UsbCamera usbCamera_;
  VirtualMotionController virtualMotionController_;
  ProgramManager programManager_;
  QTimer *cameraTimer_ {nullptr};
};
#endif
