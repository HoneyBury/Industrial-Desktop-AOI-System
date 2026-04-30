#include "ui/MainWindow.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "ui_MainWindow.h"

#include <QString>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui_(new Ui::MainWindow) {
  ui_->setupUi(this);
  refreshStatus();
}

MainWindow::~MainWindow() { delete ui_; }

void MainWindow::refreshStatus() {
  ui_->statusLabel->setText(
      QStringLiteral("AOI bootstrap ready | Camera: Mac webcam | Motion: Virtual X/Y/Z/R"));
}

#endif

