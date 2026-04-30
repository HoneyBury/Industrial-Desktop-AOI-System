#include "ui/MainWindow.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "ui_MainWindow.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QTimer>

namespace {

QString formatAxisPosition(const MotionAxis axis, const std::optional<double> &position) {
  if (!position.has_value()) {
    return QStringLiteral("--");
  }

  return axis == MotionAxis::R ? QStringLiteral("%1 deg").arg(position.value(), 0, 'f', 3)
                               : QStringLiteral("%1 mm").arg(position.value(), 0, 'f', 3);
}

QImage frameToImage(const CameraFrame &frame) {
  if (frame.width <= 0 || frame.height <= 0 || frame.data.empty()) {
    return {};
  }

  if (frame.pixelFormat == CameraPixelFormat::Gray8) {
    return QImage(frame.data.data(), frame.width, frame.height, frame.width, QImage::Format_Grayscale8)
        .copy();
  }

  const int bytesPerLine = frame.width * frame.channels;
  QImage image(frame.data.data(), frame.width, frame.height, bytesPerLine, QImage::Format_RGB888);
  if (frame.pixelFormat == CameraPixelFormat::Bgr24) {
    return image.rgbSwapped().copy();
  }

  return image.copy();
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui_(new Ui::MainWindow) {
  ui_->setupUi(this);
  cameraTimer_ = new QTimer(this);
  cameraTimer_->setInterval(80);

  bindCameraControls();
  bindMotionControls();
  bindProgramControls();
  appendLog(QStringLiteral("系统启动完成，虚拟运动控制面板已加载。"));
  appendLog(QStringLiteral("当前演示环境：Mac 摄像头 + 虚拟 X/Y/Z/R 四轴平台。"));
  createDefaultProgram();
  refreshCameraPanel();
  refreshStatus();
  refreshMotionPanel();
}

MainWindow::~MainWindow() {
  stopCameraPreview();
  delete ui_;
}

void MainWindow::bindCameraControls() {
  connect(ui_->startCameraButton, &QPushButton::clicked, this, &MainWindow::startCameraPreview);
  connect(ui_->stopCameraButton, &QPushButton::clicked, this, &MainWindow::stopCameraPreview);
  connect(cameraTimer_, &QTimer::timeout, this, &MainWindow::updateCameraPreview);
}

void MainWindow::bindMotionControls() {
  connect(ui_->xAbsMoveButton, &QPushButton::clicked, this,
          [this] { moveAxisAbsolute(MotionAxis::X); });
  connect(ui_->yAbsMoveButton, &QPushButton::clicked, this,
          [this] { moveAxisAbsolute(MotionAxis::Y); });
  connect(ui_->zAbsMoveButton, &QPushButton::clicked, this,
          [this] { moveAxisAbsolute(MotionAxis::Z); });
  connect(ui_->rAbsMoveButton, &QPushButton::clicked, this,
          [this] { moveAxisAbsolute(MotionAxis::R); });

  connect(ui_->xJogNegativeButton, &QPushButton::clicked, this,
          [this] { moveAxisRelative(MotionAxis::X, -1.0); });
  connect(ui_->xJogPositiveButton, &QPushButton::clicked, this,
          [this] { moveAxisRelative(MotionAxis::X, 1.0); });
  connect(ui_->yJogNegativeButton, &QPushButton::clicked, this,
          [this] { moveAxisRelative(MotionAxis::Y, -1.0); });
  connect(ui_->yJogPositiveButton, &QPushButton::clicked, this,
          [this] { moveAxisRelative(MotionAxis::Y, 1.0); });
  connect(ui_->zJogNegativeButton, &QPushButton::clicked, this,
          [this] { moveAxisRelative(MotionAxis::Z, -1.0); });
  connect(ui_->zJogPositiveButton, &QPushButton::clicked, this,
          [this] { moveAxisRelative(MotionAxis::Z, 1.0); });
  connect(ui_->rJogNegativeButton, &QPushButton::clicked, this,
          [this] { moveAxisRelative(MotionAxis::R, -1.0); });
  connect(ui_->rJogPositiveButton, &QPushButton::clicked, this,
          [this] { moveAxisRelative(MotionAxis::R, 1.0); });

  connect(ui_->xHomeButton, &QPushButton::clicked, this, [this] { homeAxis(MotionAxis::X); });
  connect(ui_->yHomeButton, &QPushButton::clicked, this, [this] { homeAxis(MotionAxis::Y); });
  connect(ui_->zHomeButton, &QPushButton::clicked, this, [this] { homeAxis(MotionAxis::Z); });
  connect(ui_->rHomeButton, &QPushButton::clicked, this, [this] { homeAxis(MotionAxis::R); });

  connect(ui_->emergencyStopButton, &QPushButton::clicked, this, &MainWindow::emergencyStopMotion);
  connect(ui_->resetStopButton, &QPushButton::clicked, this, &MainWindow::resetEmergencyStopMotion);
}

void MainWindow::bindProgramControls() {
  connect(ui_->newProgramButton, &QPushButton::clicked, this, &MainWindow::createDefaultProgram);
  connect(ui_->loadProgramButton, &QPushButton::clicked, this, &MainWindow::loadDefaultProgram);
  connect(ui_->saveProgramButton, &QPushButton::clicked, this, &MainWindow::saveCurrentProgram);
}

void MainWindow::refreshCameraPanel() {
  const bool cameraOpened = usbCamera_.isOpened();
  ui_->cameraSourceValueLabel->setText(cameraModeText());
  ui_->cameraStatusValueLabel->setText(cameraOpened ? QStringLiteral("预览中")
                                                    : QStringLiteral("未启动"));
  ui_->cameraStatusValueLabel->setStyleSheet(
      cameraOpened ? QStringLiteral("color: #067647; font-weight: 700;")
                   : QStringLiteral("color: #667085; font-weight: 700;"));
  ui_->startCameraButton->setEnabled(!cameraOpened);
  ui_->stopCameraButton->setEnabled(cameraOpened);
  ui_->cameraIndexSpinBox->setEnabled(!cameraOpened);

  if (!cameraOpened) {
    ui_->cameraFrameInfoValueLabel->setText(QStringLiteral("--"));
    ui_->cameraPreviewLabel->setPixmap(QPixmap());
    ui_->cameraPreviewLabel->setText(QStringLiteral("点击“开始预览”以打开摄像头或模拟画面"));
  }
}

void MainWindow::refreshStatus() {
  const QString stopState =
      virtualMotionController_.isStopped() ? QStringLiteral("已急停") : QStringLiteral("运行就绪");
  const QString cameraState = usbCamera_.isOpened() ? QStringLiteral("预览中") : QStringLiteral("未启动");
  ui_->statusLabel->setText(
      QStringLiteral("系统状态：%1 | 相机：%2 | 运动：虚拟 X/Y/Z/R 轴").arg(stopState, cameraState));
  ui_->motionStateValueLabel->setText(stopState);
  ui_->motionStateValueLabel->setStyleSheet(virtualMotionController_.isStopped()
                                                ? QStringLiteral("color: #b42318; font-weight: 700;")
                                                : QStringLiteral("color: #067647; font-weight: 700;"));
}

void MainWindow::refreshMotionPanel() {
  const bool axisOperationsEnabled = !virtualMotionController_.isStopped();

  for (const MotionAxis axis : {MotionAxis::X, MotionAxis::Y, MotionAxis::Z, MotionAxis::R}) {
    positionLabel(axis)->setText(formatAxisPosition(axis, virtualMotionController_.position(axis)));
    axisStateLabel(axis)->setText(virtualMotionController_.isStopped() ? QStringLiteral("急停锁定")
                                                                       : QStringLiteral("可操作"));
    axisStateLabel(axis)->setStyleSheet(virtualMotionController_.isStopped()
                                            ? QStringLiteral("color: #b42318;")
                                            : QStringLiteral("color: #344054;"));
    targetSpinBox(axis)->setEnabled(axisOperationsEnabled);
    stepSpinBox(axis)->setEnabled(axisOperationsEnabled);
  }

  ui_->xAbsMoveButton->setEnabled(axisOperationsEnabled);
  ui_->yAbsMoveButton->setEnabled(axisOperationsEnabled);
  ui_->zAbsMoveButton->setEnabled(axisOperationsEnabled);
  ui_->rAbsMoveButton->setEnabled(axisOperationsEnabled);
  ui_->xJogNegativeButton->setEnabled(axisOperationsEnabled);
  ui_->xJogPositiveButton->setEnabled(axisOperationsEnabled);
  ui_->yJogNegativeButton->setEnabled(axisOperationsEnabled);
  ui_->yJogPositiveButton->setEnabled(axisOperationsEnabled);
  ui_->zJogNegativeButton->setEnabled(axisOperationsEnabled);
  ui_->zJogPositiveButton->setEnabled(axisOperationsEnabled);
  ui_->rJogNegativeButton->setEnabled(axisOperationsEnabled);
  ui_->rJogPositiveButton->setEnabled(axisOperationsEnabled);
  ui_->xHomeButton->setEnabled(axisOperationsEnabled);
  ui_->yHomeButton->setEnabled(axisOperationsEnabled);
  ui_->zHomeButton->setEnabled(axisOperationsEnabled);
  ui_->rHomeButton->setEnabled(axisOperationsEnabled);
  ui_->emergencyStopButton->setEnabled(!virtualMotionController_.isStopped());
  ui_->resetStopButton->setEnabled(virtualMotionController_.isStopped());
  refreshStatus();
}

void MainWindow::refreshProgramSummary() {
  const auto currentProgram = programManager_.currentProgram();
  if (!currentProgram.has_value()) {
    ui_->programNameValueLabel->setText(QStringLiteral("未加载"));
    ui_->programFileValueLabel->setText(QStringLiteral("未关联文件"));
    ui_->programAiModelValueLabel->setText(QStringLiteral("--"));
    ui_->programMarksValueLabel->setText(QStringLiteral("0"));
    ui_->programRoisValueLabel->setText(QStringLiteral("0"));
    return;
  }

  ui_->programNameValueLabel->setText(QString::fromStdString(currentProgram->name));
  ui_->programFileValueLabel->setText(
      currentProgram->filePath.empty() ? QStringLiteral("内存中的默认程序")
                                       : QString::fromStdString(currentProgram->filePath));
  ui_->programAiModelValueLabel->setText(QString::fromStdString(currentProgram->aiModelPath));
  ui_->programMarksValueLabel->setText(QString::number(currentProgram->marks.size()));
  ui_->programRoisValueLabel->setText(QString::number(currentProgram->rois.size()));
}

void MainWindow::startCameraPreview() {
  const int cameraIndex = ui_->cameraIndexSpinBox->value();
  if (!usbCamera_.open(cameraIndex)) {
    appendLog(QStringLiteral("相机打开失败，索引：%1。").arg(cameraIndex));
    refreshCameraPanel();
    refreshStatus();
    return;
  }

  appendLog(QStringLiteral("相机预览已启动，模式：%1，索引：%2。")
                .arg(cameraModeText())
                .arg(cameraIndex));
  cameraTimer_->start();
  refreshCameraPanel();
  updateCameraPreview();
  refreshStatus();
}

void MainWindow::stopCameraPreview() {
  if (!usbCamera_.isOpened()) {
    refreshCameraPanel();
    refreshStatus();
    return;
  }

  cameraTimer_->stop();
  usbCamera_.close();
  appendLog(QStringLiteral("相机预览已停止。"));
  refreshCameraPanel();
  refreshStatus();
}

void MainWindow::updateCameraPreview() {
  const CameraFrame frame = usbCamera_.grabFrame();
  const QImage image = frameToImage(frame);
  if (image.isNull()) {
    ui_->cameraFrameInfoValueLabel->setText(QStringLiteral("取帧失败"));
    ui_->cameraPreviewLabel->setPixmap(QPixmap());
    ui_->cameraPreviewLabel->setText(QStringLiteral("当前没有可显示的画面"));
    return;
  }

  ui_->cameraPreviewLabel->setText(QString());
  ui_->cameraPreviewLabel->setPixmap(QPixmap::fromImage(image).scaled(
      ui_->cameraPreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  ui_->cameraFrameInfoValueLabel->setText(
      QStringLiteral("%1 x %2 / %3 ch").arg(frame.width).arg(frame.height).arg(frame.channels));
}

void MainWindow::appendLog(const QString &message) {
  const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
  ui_->consoleTextEdit->append(QStringLiteral("[%1] %2").arg(timestamp, message));
}

void MainWindow::moveAxisAbsolute(const MotionAxis axis) {
  const double target = targetSpinBox(axis)->value();
  if (virtualMotionController_.moveAbsolute(axis, target)) {
    appendLog(QStringLiteral("%1 轴绝对移动到 %2。").arg(axisName(axis)).arg(target, 0, 'f', 3));
  } else {
    appendLog(QStringLiteral("%1 轴绝对移动失败，当前处于急停状态。").arg(axisName(axis)));
  }

  refreshMotionPanel();
}

void MainWindow::moveAxisRelative(const MotionAxis axis, const double direction) {
  const double delta = stepSpinBox(axis)->value() * direction;
  if (virtualMotionController_.moveRelative(axis, delta)) {
    appendLog(QStringLiteral("%1 轴相对移动 %2。").arg(axisName(axis)).arg(delta, 0, 'f', 3));
  } else {
    appendLog(QStringLiteral("%1 轴点动失败，当前处于急停状态。").arg(axisName(axis)));
  }

  refreshMotionPanel();
}

void MainWindow::homeAxis(const MotionAxis axis) {
  if (virtualMotionController_.home(axis)) {
    appendLog(QStringLiteral("%1 轴已回零。").arg(axisName(axis)));
  } else {
    appendLog(QStringLiteral("%1 轴回零失败，当前处于急停状态。").arg(axisName(axis)));
  }

  refreshMotionPanel();
}

void MainWindow::emergencyStopMotion() {
  virtualMotionController_.emergencyStop();
  appendLog(QStringLiteral("已触发急停，所有轴进入锁定状态。"));
  refreshMotionPanel();
}

void MainWindow::resetEmergencyStopMotion() {
  virtualMotionController_.resetEmergencyStop();
  appendLog(QStringLiteral("急停已复位，虚拟运动平台恢复可操作状态。"));
  refreshMotionPanel();
}

void MainWindow::createDefaultProgram() {
  const auto result = programManager_.createDefaultProgram();
  if (result) {
    appendLog(QStringLiteral("已创建默认 AOI 程序模板。"));
  } else {
    appendLog(QStringLiteral("默认程序创建失败：%1").arg(QString::fromStdString(result.message)));
  }

  refreshProgramSummary();
}

void MainWindow::loadDefaultProgram() {
  const QString filePath = projectFilePath(QStringLiteral("config/default_program.json"));
  const auto result = programManager_.loadProgram(filePath.toStdString());
  if (result) {
    appendLog(QStringLiteral("已从 %1 加载程序。").arg(filePath));
  } else {
    appendLog(QStringLiteral("程序加载失败：%1").arg(QString::fromStdString(result.message)));
  }

  refreshProgramSummary();
}

void MainWindow::saveCurrentProgram() {
  const QString filePath = projectFilePath(QStringLiteral("data/active_demo_program.json"));
  QDir().mkpath(QFileInfo(filePath).absolutePath());

  const auto result = programManager_.saveProgram(filePath.toStdString());
  if (result) {
    if (const auto currentProgram = programManager_.currentProgram(); currentProgram.has_value()) {
      ProgramModel updatedProgram = *currentProgram;
      updatedProgram.filePath = filePath.toStdString();
      programManager_.createProgram(updatedProgram);
    }

    appendLog(QStringLiteral("当前程序已保存到 %1。").arg(filePath));
  } else {
    appendLog(QStringLiteral("程序保存失败：%1").arg(QString::fromStdString(result.message)));
  }

  refreshProgramSummary();
}

QString MainWindow::axisName(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::X:
    return QStringLiteral("X");
  case MotionAxis::Y:
    return QStringLiteral("Y");
  case MotionAxis::Z:
    return QStringLiteral("Z");
  case MotionAxis::R:
    return QStringLiteral("R");
  }

  return QStringLiteral("Unknown");
}

QString MainWindow::cameraModeText() const {
#ifdef AOI_HAS_OPENCV
  return QStringLiteral("Mac 摄像头实时采集");
#else
  return QStringLiteral("无 OpenCV 环境下的模拟预览");
#endif
}

QString MainWindow::projectRootPath() const {
  const QString applicationDir = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      QDir::currentPath(),
      applicationDir,
      QDir(applicationDir).absoluteFilePath(QStringLiteral("..")),
      QDir(applicationDir).absoluteFilePath(QStringLiteral("../..")),
      QDir(applicationDir).absoluteFilePath(QStringLiteral("../../..")),
  };

  for (const QString &candidate : candidates) {
    const QFileInfo defaultProgramFile(QDir(candidate).filePath(QStringLiteral("config/default_program.json")));
    if (defaultProgramFile.exists()) {
      return QDir(candidate).absolutePath();
    }
  }

  return QDir::currentPath();
}

QString MainWindow::projectFilePath(const QString &relativePath) const {
  return QDir(projectRootPath()).filePath(relativePath);
}

QDoubleSpinBox *MainWindow::targetSpinBox(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::X:
    return ui_->xTargetSpinBox;
  case MotionAxis::Y:
    return ui_->yTargetSpinBox;
  case MotionAxis::Z:
    return ui_->zTargetSpinBox;
  case MotionAxis::R:
    return ui_->rTargetSpinBox;
  }

  return ui_->xTargetSpinBox;
}

QDoubleSpinBox *MainWindow::stepSpinBox(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::X:
    return ui_->xStepSpinBox;
  case MotionAxis::Y:
    return ui_->yStepSpinBox;
  case MotionAxis::Z:
    return ui_->zStepSpinBox;
  case MotionAxis::R:
    return ui_->rStepSpinBox;
  }

  return ui_->xStepSpinBox;
}

QLabel *MainWindow::positionLabel(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::X:
    return ui_->xPositionValueLabel;
  case MotionAxis::Y:
    return ui_->yPositionValueLabel;
  case MotionAxis::Z:
    return ui_->zPositionValueLabel;
  case MotionAxis::R:
    return ui_->rPositionValueLabel;
  }

  return ui_->xPositionValueLabel;
}

QLabel *MainWindow::axisStateLabel(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::X:
    return ui_->xAxisStateValueLabel;
  case MotionAxis::Y:
    return ui_->yAxisStateValueLabel;
  case MotionAxis::Z:
    return ui_->zAxisStateValueLabel;
  case MotionAxis::R:
    return ui_->rAxisStateValueLabel;
  }

  return ui_->xAxisStateValueLabel;
}

#endif
