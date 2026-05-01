#include <QApplication>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif

#include "HostSpcExampleWindow.h"
#include "infrastructure/Logger.h"
#include "infrastructure/RuntimeDiagnostics.h"

int main(int argc, char* argv[]) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#endif
    LaserSpc::Infrastructure::RuntimeDiagnostics::prepareQtRuntime();
    QApplication app(argc, argv);
    app.setApplicationName(QObject::tr("LaserSpcHostWriterExample"));
    app.setOrganizationName(QObject::tr("LaserSpc"));

    for (const QString& warning : LaserSpc::Infrastructure::RuntimeDiagnostics::startupWarnings()) {
        LaserSpc::Infrastructure::Logger::warn(warning);
    }

    HostSpc::HostSpcExampleWindow window;
    window.show();
    return app.exec();
}
