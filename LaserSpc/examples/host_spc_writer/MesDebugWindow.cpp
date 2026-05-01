#include "MesDebugWindow.h"

#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVBoxLayout>
#include <QWidget>

#include "infrastructure/MesEventForwarder.h"

namespace HostSpc {

namespace {

QString buildBoardCode() {
    return QObject::tr("MES-%1").arg(QDateTime::currentDateTimeUtc().toString(QObject::tr("yyyyMMddHHmmss")));
}

}  // namespace

MesDebugWindow::MesDebugWindow(const LaserSpc::Infrastructure::MesSettings& settings, QWidget* parent)
    : QMainWindow(parent), m_settings(settings), m_server(new QTcpServer(this)) {
    buildUi();
    connect(m_server, &QTcpServer::newConnection, this, &MesDebugWindow::handleNewConnection);
    refreshReceiverStatus();
}

MesDebugWindow::~MesDebugWindow() = default;

void MesDebugWindow::setSampleContext(const QString& lineName,
                                      const QString& programName,
                                      const QString& deviceName,
                                      const QString& operatorName) {
    m_lineNameEdit->setText(lineName);
    m_programNameEdit->setText(programName);
    m_deviceNameEdit->setText(deviceName);
    m_operatorNameEdit->setText(operatorName);
}

void MesDebugWindow::buildUi() {
    setWindowTitle(QObject::tr("MES 调试窗口"));
    resize(980, 760);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    auto* receiverGroup = new QGroupBox(QObject::tr("本地接收器"), central);
    auto* receiverLayout = new QFormLayout(receiverGroup);
    m_portSpinBox = new QSpinBox(receiverGroup);
    m_portSpinBox->setRange(1024, 65535);
    m_portSpinBox->setValue(18119);
    m_toggleReceiverButton = new QPushButton(QObject::tr("启动接收器"), receiverGroup);
    auto* receiverButtonRow = new QWidget(receiverGroup);
    auto* receiverButtonLayout = new QHBoxLayout(receiverButtonRow);
    receiverButtonLayout->setContentsMargins(0, 0, 0, 0);
    receiverButtonLayout->setSpacing(8);
    receiverButtonLayout->addWidget(m_toggleReceiverButton);
    receiverButtonLayout->addStretch();
    m_receiverStatusLabel = new QLabel(receiverGroup);
    m_receiverStatusLabel->setWordWrap(true);
    m_endpointHintLabel = new QLabel(receiverGroup);
    m_endpointHintLabel->setWordWrap(true);
    receiverLayout->addRow(QObject::tr("监听端口"), m_portSpinBox);
    receiverLayout->addRow(QString(), receiverButtonRow);
    receiverLayout->addRow(QObject::tr("接收状态"), m_receiverStatusLabel);
    receiverLayout->addRow(QObject::tr("本地地址"), m_endpointHintLabel);

    auto* senderGroup = new QGroupBox(QObject::tr("MES 转发参数"), central);
    auto* senderLayout = new QFormLayout(senderGroup);
    m_endpointUrlEdit = new QLineEdit(senderGroup);
    m_endpointUrlEdit->setText(m_settings.endpointUrl);
    m_siteCodeEdit = new QLineEdit(m_settings.siteCode, senderGroup);
    m_stationCodeEdit = new QLineEdit(m_settings.stationCode, senderGroup);
    m_userNameEdit = new QLineEdit(m_settings.userName, senderGroup);
    auto* senderButtonRow = new QWidget(senderGroup);
    auto* senderButtonLayout = new QHBoxLayout(senderButtonRow);
    senderButtonLayout->setContentsMargins(0, 0, 0, 0);
    senderButtonLayout->setSpacing(8);
    auto* useLocalReceiverButton = new QPushButton(QObject::tr("使用本地接收器地址"), senderButtonRow);
    auto* sendSampleButton = new QPushButton(QObject::tr("发送示例批次"), senderButtonRow);
    senderButtonLayout->addWidget(useLocalReceiverButton);
    senderButtonLayout->addWidget(sendSampleButton);
    senderButtonLayout->addStretch();
    senderLayout->addRow(QObject::tr("MES 地址"), m_endpointUrlEdit);
    senderLayout->addRow(QObject::tr("站点编码"), m_siteCodeEdit);
    senderLayout->addRow(QObject::tr("工位编码"), m_stationCodeEdit);
    senderLayout->addRow(QObject::tr("用户"), m_userNameEdit);
    senderLayout->addRow(QString(), senderButtonRow);

    auto* sampleGroup = new QGroupBox(QObject::tr("示例数据"), central);
    auto* sampleLayout = new QFormLayout(sampleGroup);
    m_boardCodeEdit = new QLineEdit(buildBoardCode(), sampleGroup);
    m_lineNameEdit = new QLineEdit(QObject::tr("L1"), sampleGroup);
    m_programNameEdit = new QLineEdit(QObject::tr("Program-A"), sampleGroup);
    m_deviceNameEdit = new QLineEdit(QObject::tr("Laser-01"), sampleGroup);
    m_operatorNameEdit = new QLineEdit(QObject::tr("MesDebug"), sampleGroup);
    m_pointCountSpin = new QSpinBox(sampleGroup);
    m_pointCountSpin->setRange(1, 20);
    m_pointCountSpin->setValue(3);
    sampleLayout->addRow(QObject::tr("板号"), m_boardCodeEdit);
    sampleLayout->addRow(QObject::tr("线体"), m_lineNameEdit);
    sampleLayout->addRow(QObject::tr("程序"), m_programNameEdit);
    sampleLayout->addRow(QObject::tr("设备"), m_deviceNameEdit);
    sampleLayout->addRow(QObject::tr("操作员"), m_operatorNameEdit);
    sampleLayout->addRow(QObject::tr("点位数量"), m_pointCountSpin);

    auto* outputGroup = new QGroupBox(QObject::tr("接收内容 / 日志"), central);
    auto* outputLayout = new QHBoxLayout(outputGroup);
    m_payloadEdit = new QPlainTextEdit(outputGroup);
    m_payloadEdit->setReadOnly(true);
    m_payloadEdit->setPlaceholderText(QObject::tr("这里会显示最近一次收到的 MES 请求体。"));
    m_logEdit = new QPlainTextEdit(outputGroup);
    m_logEdit->setReadOnly(true);
    m_logEdit->setPlaceholderText(QObject::tr("这里会显示发送和接收日志。"));
    outputLayout->addWidget(m_payloadEdit, 3);
    outputLayout->addWidget(m_logEdit, 2);

    rootLayout->addWidget(receiverGroup);
    rootLayout->addWidget(senderGroup);
    rootLayout->addWidget(sampleGroup);
    rootLayout->addWidget(outputGroup, 1);
    setCentralWidget(central);

    connect(m_toggleReceiverButton, &QPushButton::clicked, this, &MesDebugWindow::toggleReceiver);
    connect(useLocalReceiverButton, &QPushButton::clicked, this, &MesDebugWindow::useLocalReceiverEndpoint);
    connect(sendSampleButton, &QPushButton::clicked, this, &MesDebugWindow::sendSampleBatch);
}

void MesDebugWindow::toggleReceiver() {
    if (m_server->isListening()) {
        m_server->close();
        appendLog(QObject::tr("本地 MES 接收器已停止。"));
        refreshReceiverStatus();
        return;
    }

    if (!m_server->listen(QHostAddress::Any, static_cast<quint16>(m_portSpinBox->value()))) {
        appendLog(QObject::tr("启动接收器失败：%1").arg(m_server->errorString()));
        refreshReceiverStatus();
        return;
    }

    appendLog(QObject::tr("本地 MES 接收器已启动：%1").arg(m_server->serverPort()));
    refreshReceiverStatus();
}

void MesDebugWindow::handleNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &MesDebugWindow::handleClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void MesDebugWindow::handleClientReadyRead() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == nullptr) {
        return;
    }

    const QByteArray rawRequest = socket->readAll();
    const int bodyIndex = rawRequest.indexOf("\r\n\r\n");
    const QByteArray body = bodyIndex >= 0 ? rawRequest.mid(bodyIndex + 4) : rawRequest;
    m_payloadEdit->setPlainText(QString::fromUtf8(body));
    appendLog(QObject::tr("收到本地 MES 请求：%1 字节").arg(body.size()));

    const QByteArray response =
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 16\r\nConnection: close\r\n\r\n{\"success\":true}";
    socket->write(response);
    socket->disconnectFromHost();
}

void MesDebugWindow::sendSampleBatch() {
    LaserSpc::Infrastructure::MesSettings settings;
    settings.enabled = true;
    settings.endpointUrl = m_endpointUrlEdit->text().trimmed();
    settings.siteCode = m_siteCodeEdit->text().trimmed();
    settings.stationCode = m_stationCodeEdit->text().trimmed();
    settings.userName = m_userNameEdit->text().trimmed();
    settings.timeoutSeconds = 5;

    LaserSpc::Infrastructure::MesEventForwarder forwarder(settings);
    QString errorMessage;
    const auto batch = buildSampleBatch();
    const bool success = forwarder.forwardInspectionBatch(batch, &errorMessage);
    if (success) {
        appendLog(QObject::tr("MES 发送成功 | requestId=%1 | board=%2 | points=%3")
                      .arg(batch.requestId, batch.board.boardCode)
                      .arg(batch.points.size()));
    } else {
        appendLog(QObject::tr("MES 发送失败 | requestId=%1 | error=%2").arg(batch.requestId, errorMessage));
    }
}

void MesDebugWindow::useLocalReceiverEndpoint() {
    if (!m_server->isListening()) {
        toggleReceiver();
    }
    if (m_server->isListening()) {
        m_endpointUrlEdit->setText(QObject::tr("http://127.0.0.1:%1").arg(m_server->serverPort()));
    }
    refreshReceiverStatus();
}

void MesDebugWindow::refreshReceiverStatus() {
    if (m_server->isListening()) {
        m_receiverStatusLabel->setText(QObject::tr("运行中"));
        m_endpointHintLabel->setText(QObject::tr("http://127.0.0.1:%1").arg(m_server->serverPort()));
        m_toggleReceiverButton->setText(QObject::tr("停止接收器"));
        return;
    }

    m_receiverStatusLabel->setText(QObject::tr("未启动"));
    m_endpointHintLabel->setText(QObject::tr("http://127.0.0.1:%1").arg(m_portSpinBox->value()));
    m_toggleReceiverButton->setText(QObject::tr("启动接收器"));
}

void MesDebugWindow::appendLog(const QString& message) {
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(QObject::tr("HH:mm:ss.zzz"));
    m_logEdit->appendPlainText(QObject::tr("[%1] %2").arg(timestamp, message));
}

LaserSpc::Domain::InspectionBatch MesDebugWindow::buildSampleBatch() const {
    LaserSpc::Domain::InspectionBatch batch;
    batch.requestId = QObject::tr("mes-debug-%1").arg(QDateTime::currentDateTimeUtc().toString(QObject::tr("yyyyMMddHHmmsszzz")));
    batch.board.boardCode = m_boardCodeEdit->text().trimmed();
    batch.board.result = QObject::tr("NG");
    batch.board.lineName = m_lineNameEdit->text().trimmed();
    batch.board.programName = m_programNameEdit->text().trimmed();
    batch.board.deviceName = m_deviceNameEdit->text().trimmed();
    batch.board.operatorName = m_operatorNameEdit->text().trimmed();
    batch.board.eventTime = QDateTime::currentDateTimeUtc();

    for (int index = 1; index <= m_pointCountSpin->value(); ++index) {
        LaserSpc::Domain::PointRecordRow point;
        point.boardCode = batch.board.boardCode;
        point.pointName = QObject::tr("P%1").arg(index, 2, 10, QChar('0'));
        point.result = index == m_pointCountSpin->value() ? QObject::tr("NG") : QObject::tr("OK");
        point.readGrade = point.result == QObject::tr("OK") ? QObject::tr("A") : QObject::tr("C");
        point.readCodeContent = QObject::tr("%1-%2").arg(batch.board.boardCode, point.pointName);
        point.isLaser = true;
        point.isReadCode = true;
        point.lineName = batch.board.lineName;
        point.programName = batch.board.programName;
        point.deviceName = batch.board.deviceName;
        point.startTime = batch.board.eventTime.addSecs(-index);
        point.endTime = batch.board.eventTime.addSecs(index);
        batch.points.append(point);
    }
    return batch;
}

}  // namespace HostSpc
