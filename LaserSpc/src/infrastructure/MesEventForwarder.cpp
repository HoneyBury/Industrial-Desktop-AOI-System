#include "infrastructure/MesEventForwarder.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace LaserSpc::Infrastructure {

MesEventForwarder::MesEventForwarder(MesSettings settings) : m_settings(std::move(settings)) {}

bool MesEventForwarder::forwardInspectionBatch(const LaserSpc::Domain::InspectionBatch& batch, QString* errorMessage) const {
    if (!m_settings.enabled || m_settings.endpointUrl.trimmed().isEmpty()) {
        return true;
    }

    QJsonObject board{
        {"boardCode", batch.board.boardCode},
        {"result", batch.board.result},
        {"lineName", batch.board.lineName},
        {"programName", batch.board.programName},
        {"deviceName", batch.board.deviceName},
        {"operatorName", batch.board.operatorName},
        {"eventTime", batch.board.eventTime.toString(Qt::ISODate)}
    };

    QJsonArray points;
    for (const auto& point : batch.points) {
        points.append(QJsonObject{
            {"boardCode", point.boardCode},
            {"pointName", point.pointName},
            {"result", point.result},
            {"readGrade", point.readGrade},
            {"isLaser", point.isLaser},
            {"isReadCode", point.isReadCode},
            {"lineName", point.lineName},
            {"programName", point.programName},
            {"deviceName", point.deviceName},
            {"startTime", point.startTime.toString(Qt::ISODate)},
            {"endTime", point.endTime.toString(Qt::ISODate)}
        });
    }

    QJsonObject payload{
        {"requestId", batch.requestId},
        {"siteCode", m_settings.siteCode},
        {"stationCode", m_settings.stationCode},
        {"userName", m_settings.userName},
        {"board", board},
        {"points", points}
    };

    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(m_settings.endpointUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QObject::tr("application/json"));
    request.setRawHeader("X-LaserSpc-Source", "LaserSpcIngestServer");

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QNetworkReply* reply = manager.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(qMax(1, m_settings.timeoutSeconds) * 1000);
    loop.exec();

    if (timer.isActive()) {
        timer.stop();
    } else {
        reply->abort();
        if (errorMessage) {
            *errorMessage = QObject::tr("MES forward timed out.");
        }
        reply->deleteLater();
        return false;
    }

    const bool success = reply->error() == QNetworkReply::NoError &&
                         reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() < 400;
    if (!success && errorMessage) {
        *errorMessage = reply->errorString();
    }
    reply->deleteLater();
    return success;
}

}  // namespace LaserSpc::Infrastructure
