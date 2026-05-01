#include "spc/LaserSpcWindow.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "app/AppServiceFacade.h"
#include "infrastructure/AppConfigService.h"
#include "infrastructure/RepositoryFactory.h"
#include "ui/shell/DashboardWidget.h"

#include <QVBoxLayout>

LaserSpcWindow::LaserSpcWindow(QWidget *parent) : QWidget(parent, Qt::Window) {
  setWindowTitle(QString::fromUtf8("Laser SPC — 镭雕统计过程控制"));
  resize(1100, 750);
  buildUi();
}

LaserSpcWindow::~LaserSpcWindow() = default;

void LaserSpcWindow::buildUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  LaserSpc::Infrastructure::AppConfigService configService;
  auto settings = configService.settings();

  // On macOS without MySQL, allow the mock fallback so the dashboard still
  // renders with seed data.  When MySQL is available the repository will use
  // it automatically.
  settings.allowMockFallback = true;

  auto buildResult =
      LaserSpc::Infrastructure::RepositoryFactory::build(settings);

  facade_ = std::make_unique<LaserSpc::App::AppServiceFacade>(
      std::move(buildResult.repository), buildResult.dataSourceMode);

  dashboard_ = new LaserSpc::Ui::DashboardWidget(facade_.get(), this);
  layout->addWidget(dashboard_);
}

void LaserSpcWindow::refreshDashboard() {
  if (dashboard_ != nullptr) {
    dashboard_->refreshData();
  }
}

#endif // AOI_HAS_QT_WIDGETS
