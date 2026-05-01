#include "ui/shell/MainWindow.h"

#include "ui/shell/DashboardWidget.h"

namespace LaserSpc::Ui {

MainWindow::MainWindow(LaserSpc::App::AppServiceFacade* facade, QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QObject::tr("LaserSpc - SPC Dashboard"));
    resize(1440, 900);
    m_dashboard = new DashboardWidget(facade, this);
    setCentralWidget(m_dashboard);
}

void MainWindow::refreshData() {
    if (m_dashboard != nullptr) {
        m_dashboard->refreshData();
    }
}

void MainWindow::refreshFilterOptions() {
    if (m_dashboard != nullptr) {
        m_dashboard->refreshFilterOptions();
    }
}

DashboardWidget* MainWindow::dashboardWidget() const {
    return m_dashboard;
}

}  // namespace LaserSpc::Ui
