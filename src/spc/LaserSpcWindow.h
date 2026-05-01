#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include <QWidget>

#include <memory>

namespace LaserSpc::App {
class AppServiceFacade;
}

namespace LaserSpc::Ui {
class DashboardWidget;
}

/// A top-level window that embeds the LaserSpc DashboardWidget.
/// Opened from the MainWindow SPC button.
class LaserSpcWindow final : public QWidget {
  Q_OBJECT

public:
  explicit LaserSpcWindow(QWidget *parent = nullptr);
  ~LaserSpcWindow() override;

  /// Trigger a data refresh on the embedded dashboard.
  void refreshDashboard();

private:
  void buildUi();

  std::unique_ptr<LaserSpc::App::AppServiceFacade> facade_;
  LaserSpc::Ui::DashboardWidget *dashboard_ = nullptr;
};

#endif // AOI_HAS_QT_WIDGETS
