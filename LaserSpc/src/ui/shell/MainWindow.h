#pragma once

#include <QMainWindow>

namespace LaserSpc::App {
class AppServiceFacade;
}

namespace LaserSpc::Ui {
class DashboardWidget;
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(LaserSpc::App::AppServiceFacade* facade, QWidget* parent = nullptr);
    void refreshData();
    void refreshFilterOptions();
    DashboardWidget* dashboardWidget() const;

private:
    DashboardWidget* m_dashboard = nullptr;
};

}  // namespace LaserSpc::Ui
