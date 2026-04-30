#include "ui/MainWindow.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "ui_MainWindow.h"

#include <QDateTime>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>

namespace {

QString formatAxisPosition(const MotionAxis axis, const std::optional<double> &position) {
  if (!position.has_value()) {
    return QStringLiteral("--");
  }

  return axis == MotionAxis::R ? QStringLiteral("%1 deg").arg(position.value(), 0, 'f', 3)
                               : QStringLiteral("%1 mm").arg(position.value(), 0, 'f', 3);
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui_(new Ui::MainWindow) {
  ui_->setupUi(this);
  bindMotionControls();
  appendLog(QStringLiteral("系统启动完成，虚拟运动控制面板已加载。"));
  appendLog(QStringLiteral("当前演示环境：Mac 摄像头 + 虚拟 X/Y/Z/R 四轴平台。"));
  refreshStatus();
  refreshMotionPanel();
}

MainWindow::~MainWindow() { delete ui_; }

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

void MainWindow::refreshStatus() {
  const QString stopState =
      virtualMotionController_.isStopped() ? QStringLiteral("已急停") : QStringLiteral("运行就绪");
  ui_->statusLabel->setText(
      QStringLiteral("系统状态：%1 | 相机：Mac 摄像头模拟 | 运动：虚拟 X/Y/Z/R 轴")
          .arg(stopState));
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
