#include <QCoreApplication>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>

#include "app/IngestService.h"
#include "infrastructure/AppConfigService.h"
#include "infrastructure/Logger.h"
#include "infrastructure/MesEventForwarder.h"
#include "infrastructure/MySqlSpcRepository.h"
#include "infrastructure/MySqlSpcWriteRepository.h"

namespace {

QString envOrDefault(const char* name, const QString& fallback) {
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() ? fallback : value;
}

int envIntOrDefault(const char* name, int fallback) {
    const int value = qEnvironmentVariableIntValue(name);
    return value <= 0 ? fallback : value;
}

QDateTime parseDateTime(const QJsonValue& value) {
    if (!value.isString()) {
        return {};
    }
    const QString text = value.toString().trimmed();
    QDateTime dateTime = QDateTime::fromString(text, Qt::ISODate);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(text, QObject::tr("yyyy-MM-dd HH:mm:ss"));
    }
    return dateTime;
}

LaserSpc::Domain::BoardRecordRow parseBoardRow(const QJsonObject& object) {
    LaserSpc::Domain::BoardRecordRow row;
    row.boardCode = object.value("boardCode").toString();
    row.result = object.value("result").toString();
    row.lineName = object.value("lineName").toString();
    row.programName = object.value("programName").toString();
    row.deviceName = object.value("deviceName").toString();
    row.operatorName = object.value("operatorName").toString();
    row.eventTime = parseDateTime(object.value("eventTime"));
    return row;
}

LaserSpc::Domain::PointRecordRow parsePointRow(const QJsonObject& object) {
    LaserSpc::Domain::PointRecordRow row;
    row.boardCode = object.value("boardCode").toString();
    row.pointName = object.value("pointName").toString();
    row.result = object.value("result").toString();
    row.readGrade = object.value("readGrade").toString();
    row.laserContent = object.value("laserContent").toString();
    row.readCodeContent = object.value("readCodeContent").toString();
    row.isLaser = object.contains("isLaser") ? object.value("isLaser").toBool() : false;
    row.isReadCode = object.contains("isReadCode") ? object.value("isReadCode").toBool() : false;
    row.lineName = object.value("lineName").toString();
    row.programName = object.value("programName").toString();
    row.deviceName = object.value("deviceName").toString();
    row.startTime = parseDateTime(object.value("startTime"));
    row.endTime = parseDateTime(object.value("endTime"));
    row.detailJsonPath = object.value("detailJsonPath").toString();
    return row;
}

QByteArray jsonResponse(int statusCode, const QJsonObject& payload) {
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " OK\r\n";
    response += "Content-Type: application/json; charset=utf-8\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;
    return response;
}

int contentLength(const QByteArray& request) {
    const QList<QByteArray> lines = request.left(request.indexOf("\r\n\r\n")).split('\n');
    for (const QByteArray& rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (line.toLower().startsWith("content-length:")) {
            return line.mid(QByteArray("content-length:").size()).trimmed().toInt();
        }
    }
    return 0;
}

class IngestHttpServer : public QObject {
public:
    explicit IngestHttpServer(LaserSpc::App::IngestService service,
                              std::shared_ptr<LaserSpc::Infrastructure::MySqlSpcRepository> queryRepository,
                              QString apiKey,
                              QObject* parent = nullptr)
        : QObject(parent),
          m_service(std::move(service)),
          m_queryRepository(std::move(queryRepository)),
          m_apiKey(std::move(apiKey)) {
        connect(&m_server, &QTcpServer::newConnection, this, &IngestHttpServer::handleNewConnection);
    }

    bool listen(const QHostAddress& address, quint16 port) {
        return m_server.listen(address, port);
    }

    QString errorString() const {
        return m_server.errorString();
    }

private:
    void handleNewConnection() {
        while (m_server.hasPendingConnections()) {
            QTcpSocket* socket = m_server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { handleReadyRead(socket); });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    }

    void handleReadyRead(QTcpSocket* socket) {
        m_buffers[socket] += socket->readAll();
        const QByteArray& buffer = m_buffers[socket];
        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }

        const int expectedBody = contentLength(buffer);
        const int receivedBody = buffer.size() - headerEnd - 4;
        if (receivedBody < expectedBody) {
            return;
        }

        const QByteArray request = buffer.left(headerEnd + 4 + expectedBody);
        m_buffers.remove(socket);
        respond(socket, processRequest(request));
    }

    QByteArray processRequest(const QByteArray& request) {
        const int headerEnd = request.indexOf("\r\n\r\n");
        const QByteArray headerBlock = request.left(headerEnd);
        const QByteArray body = request.mid(headerEnd + 4);
        const QList<QByteArray> headerLines = headerBlock.split('\n');
        if (headerLines.isEmpty()) {
            return jsonResponse(400, {{"success", false}, {"error", "Malformed request."}});
        }

        const QList<QByteArray> requestLine = headerLines.first().trimmed().split(' ');
        if (requestLine.size() < 2) {
            return jsonResponse(400, {{"success", false}, {"error", "Invalid request line."}});
        }

        const QByteArray method = requestLine.at(0).trimmed().toUpper();
        const QByteArray rawPath = requestLine.at(1).trimmed();
        const QByteArray apiKey = headerValue(headerLines, "x-api-key");
        const QUrl requestUrl(QString::fromUtf8(rawPath));
        const QString path = requestUrl.path();
        const QUrlQuery queryString(requestUrl);

        if (method == "GET" && path == QStringLiteral("/health")) {
            return jsonResponse(200,
                                {{"success", true},
                                 {"status", "ok"},
                                 {"service", "LaserSpcIngestServer"},
                                 {"time", QDateTime::currentDateTime().toString(Qt::ISODate)}});
        }

        if (!m_apiKey.isEmpty() && apiKey != m_apiKey.toUtf8()) {
            return jsonResponse(401, {{"success", false}, {"error", "Unauthorized."}});
        }

        QJsonParseError error;
        const QJsonDocument json = QJsonDocument::fromJson(body, &error);
        if (method == "POST" && error.error != QJsonParseError::NoError) {
            return jsonResponse(400, {{"success", false}, {"error", "Invalid JSON body."}});
        }

        if (method == "GET" && path == QStringLiteral("/api/v1/settings-availability")) {
            return featureToggleResponse("systemSettingsEnabled", currentSettings().systemSettingsEnabled);
        }

        if (method == "POST" && path == QStringLiteral("/api/v1/settings-availability")) {
            return updateFeatureToggle(json.object(), "systemSettingsEnabled");
        }

        if (method == "GET" && path == QStringLiteral("/api/v1/export-report-availability")) {
            return featureToggleResponse("exportReportEnabled", currentSettings().exportReportEnabled);
        }

        if (method == "POST" && path == QStringLiteral("/api/v1/export-report-availability")) {
            return updateFeatureToggle(json.object(), "exportReportEnabled");
        }

        if (method == "GET" && path == QStringLiteral("/api/v1/laser-content-duplicates")) {
            const QString laserContent = queryString.queryItemValue(QStringLiteral("laserContent")).trimmed();
            if (laserContent.isEmpty()) {
                return jsonResponse(400, {{"success", false}, {"error", "laserContent is required."}});
            }
            if (m_queryRepository == nullptr) {
                return jsonResponse(500, {{"success", false}, {"error", "Query repository is unavailable."}});
            }

            const auto result = m_queryRepository->checkLaserContentDuplicate(laserContent);
            if (!m_queryRepository->lastError().isEmpty()) {
                return jsonResponse(500,
                                    {{"success", false},
                                     {"laserContent", laserContent},
                                     {"error", m_queryRepository->lastError()}});
            }

            return jsonResponse(200,
                                {{"success", true},
                                 {"laserContent", result.laserContent},
                                 {"exists", result.exists},
                                 {"duplicateCount", result.duplicateCount},
                                 {"latestBoardCode", result.latestBoardCode},
                                 {"latestPointName", result.latestPointName},
                                 {"latestEndTime", result.latestEndTime.toString(Qt::ISODate)}});
        }

        if (method == "POST" && path == QStringLiteral("/api/v1/board-records")) {
            const auto result = m_service.ingestBoardRecord(parseBoardRow(json.object()));
            return jsonResponse(result.success ? 201 : 400,
                                {{"success", result.success},
                                 {"insertedBoards", result.insertedBoards},
                                 {"insertedPoints", result.insertedPoints},
                                 {"forwardMessage", result.forwardMessage},
                                 {"error", result.errorMessage}});
        }

        if (method == "POST" && path == QStringLiteral("/api/v1/point-records")) {
            const auto result = m_service.ingestPointRecord(parsePointRow(json.object()));
            return jsonResponse(result.success ? 201 : 400,
                                {{"success", result.success},
                                 {"insertedBoards", result.insertedBoards},
                                 {"insertedPoints", result.insertedPoints},
                                 {"forwardMessage", result.forwardMessage},
                                 {"error", result.errorMessage}});
        }

        if (method == "POST" && path == QStringLiteral("/api/v1/inspection-batches")) {
            LaserSpc::Domain::InspectionBatch batch;
            const QJsonObject root = json.object();
            batch.requestId = root.value("requestId").toString();
            batch.board = parseBoardRow(root.value("board").toObject());
            for (const auto& pointValue : root.value("points").toArray()) {
                batch.points.append(parsePointRow(pointValue.toObject()));
            }
            const auto result = m_service.ingestInspectionBatch(batch);
            return jsonResponse(result.success ? 201 : 400,
                                {{"success", result.success},
                                 {"insertedBoards", result.insertedBoards},
                                 {"insertedPoints", result.insertedPoints},
                                 {"requestId", batch.requestId},
                                 {"forwardMessage", result.forwardMessage},
                                 {"error", result.errorMessage}});
        }

        return jsonResponse(404, {{"success", false}, {"error", "Endpoint not found."}});
    }

    void respond(QTcpSocket* socket, const QByteArray& payload) {
        socket->write(payload);
        socket->disconnectFromHost();
    }

    static QByteArray headerValue(const QList<QByteArray>& lines, const QByteArray& key) {
        const QByteArray needle = key.toLower() + ":";
        for (const QByteArray& rawLine : lines) {
            const QByteArray line = rawLine.trimmed();
            if (line.toLower().startsWith(needle)) {
                return line.mid(needle.size()).trimmed();
            }
        }
        return {};
    }

    LaserSpc::Infrastructure::AppSettings currentSettings() const {
        return LaserSpc::Infrastructure::AppConfigService().settings();
    }

    QByteArray featureToggleResponse(const char* key, bool enabled) const {
        return jsonResponse(200,
                            {{"success", true},
                             {"feature", QString::fromUtf8(key)},
                             {"enabled", enabled}});
    }

    QByteArray updateFeatureToggle(const QJsonObject& object, const char* key) const {
        if (!object.contains("enabled")) {
            return jsonResponse(400, {{"success", false}, {"error", "enabled is required."}});
        }

        LaserSpc::Infrastructure::AppConfigService configService;
        auto settings = configService.settings();
        const bool enabled = object.value("enabled").toBool();
        const QString featureKey = QString::fromUtf8(key);
        if (featureKey == QStringLiteral("systemSettingsEnabled")) {
            settings.systemSettingsEnabled = enabled;
        } else if (featureKey == QStringLiteral("exportReportEnabled")) {
            settings.exportReportEnabled = enabled;
        }

        QString errorMessage;
        if (!configService.saveSettings(settings, &errorMessage)) {
            return jsonResponse(500, {{"success", false}, {"error", errorMessage}});
        }

        return jsonResponse(200,
                            {{"success", true},
                             {"feature", featureKey},
                             {"enabled", enabled}});
    }

    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    LaserSpc::App::IngestService m_service;
    std::shared_ptr<LaserSpc::Infrastructure::MySqlSpcRepository> m_queryRepository;
    QString m_apiKey;
};

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    LaserSpc::Infrastructure::AppConfigService configService;
    auto settings = configService.settings();
    auto repository = std::make_shared<LaserSpc::Infrastructure::MySqlSpcWriteRepository>(settings.database);
    auto queryRepository = std::make_shared<LaserSpc::Infrastructure::MySqlSpcRepository>(settings.database);
    LaserSpc::App::IngestService service(repository);
    LaserSpc::Infrastructure::MesEventForwarder mesForwarder(settings.mes);
    service.setMesForwarder([mesForwarder](const LaserSpc::Domain::InspectionBatch& batch, QString* errorMessage) mutable {
        return mesForwarder.forwardInspectionBatch(batch, errorMessage);
    });

    const QHostAddress address(envOrDefault("LASERSPC_API_HOST", QObject::tr("0.0.0.0")));
    const quint16 port = static_cast<quint16>(envIntOrDefault("LASERSPC_API_PORT", 8099));
    const QString apiKey = envOrDefault("LASERSPC_API_KEY", QString());

    IngestHttpServer server(service, queryRepository, apiKey);
    if (!server.listen(address, port)) {
        LaserSpc::Infrastructure::Logger::error("Failed to start ingest server: " + server.errorString());
        return 1;
    }

    LaserSpc::Infrastructure::Logger::info(
        QString("LaserSpcIngestServer listening at %1:%2 apiKey=%3")
            .arg(address.toString())
            .arg(port)
            .arg(apiKey.isEmpty() ? QObject::tr("disabled") : QObject::tr("enabled")));
    return app.exec();
}
