#pragma once

#include "motion/VirtualMotionController.h"
#include "program/ProgramManager.h"

#ifdef AOI_HAS_QT_WIDGETS
#include <QMainWindow>

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
  void refreshStatus();

  Ui::MainWindow *ui_;
  VirtualMotionController virtualMotionController_;
  ProgramManager programManager_;
};
#endif

