#include <iostream>

#ifdef AOI_HAS_QT_WIDGETS
#include <QApplication>

#include "ui/MainWindow.h"
#endif

int main(int argc, char *argv[]) {
#ifdef AOI_HAS_QT_WIDGETS
  QApplication application(argc, argv);
  MainWindow window;
  window.show();
  return application.exec();
#else
  (void)argc;
  (void)argv;
  std::cout << "Industrial Desktop AOI System bootstrap build: Qt 6 not found, console mode only."
            << std::endl;
  return 0;
#endif
}

