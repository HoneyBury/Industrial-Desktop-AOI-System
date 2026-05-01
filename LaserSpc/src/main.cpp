#include <QApplication>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif

#include <memory>

#include "app/AppServiceFacade.h"
#include "infrastructure/AppConfigService.h"
#include "infrastructure/Logger.h"
#include "infrastructure/RepositoryFactory.h"
#include "infrastructure/RuntimeDiagnostics.h"
#include "ui/shell/MainWindow.h"

int main(int argc, char* argv[]) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#endif
    LaserSpc::Infrastructure::RuntimeDiagnostics::prepareQtRuntime();
    QApplication app(argc, argv);
    LaserSpc::Infrastructure::AppConfigService configService;

    for (const QString& warning : LaserSpc::Infrastructure::RuntimeDiagnostics::startupWarnings()) {
        LaserSpc::Infrastructure::Logger::warn(warning);
    }

    auto buildResult = LaserSpc::Infrastructure::RepositoryFactory::build(configService.settings());
    if (!buildResult.warningMessage.isEmpty()) {
        LaserSpc::Infrastructure::Logger::warn(buildResult.warningMessage);
    }

    LaserSpc::Infrastructure::Logger::info("Using data source: " + buildResult.dataSourceMode);

    LaserSpc::App::AppServiceFacade facade(std::move(buildResult.repository), buildResult.dataSourceMode);

    LaserSpc::Ui::MainWindow window(&facade);
    window.show();

    return app.exec();
}
