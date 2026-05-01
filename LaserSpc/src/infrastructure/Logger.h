#pragma once

#include <QDebug>
#include <QString>

namespace LaserSpc::Infrastructure {

class Logger {
public:
    static void info(const QString& message) {
        qInfo().noquote() << "[LaserSpc]" << message;
    }

    static void warn(const QString& message) {
        qWarning().noquote() << "[LaserSpc]" << message;
    }

    static void error(const QString& message) {
        qCritical().noquote() << "[LaserSpc]" << message;
    }

    static void perf(const QString& scope, qint64 elapsedMs, const QString& detail = QString()) {
        const QString suffix = detail.isEmpty() ? QString() : " | " + detail;
        qInfo().noquote() << "[LaserSpc][PERF]" << QString("%1 %2 ms%3").arg(scope).arg(elapsedMs).arg(suffix);
    }
};

}  // namespace LaserSpc::Infrastructure
