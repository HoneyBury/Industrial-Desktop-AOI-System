#pragma once

#include <QMainWindow>

#include "domain/Models.h"
#include "infrastructure/AppConfigService.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTcpServer;
class QTcpSocket;

namespace HostSpc {

class MesDebugWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MesDebugWindow(const LaserSpc::Infrastructure::MesSettings& settings, QWidget* parent = nullptr);
    ~MesDebugWindow() override;

    void setSampleContext(const QString& lineName,
                          const QString& programName,
                          const QString& deviceName,
                          const QString& operatorName);

private slots:
    void toggleReceiver();
    void handleNewConnection();
    void handleClientReadyRead();
    void sendSampleBatch();
    void useLocalReceiverEndpoint();

private:
    void buildUi();
    void refreshReceiverStatus();
    void appendLog(const QString& message);
    LaserSpc::Domain::InspectionBatch buildSampleBatch() const;

    LaserSpc::Infrastructure::MesSettings m_settings;
    QTcpServer* m_server = nullptr;
    QLabel* m_receiverStatusLabel = nullptr;
    QLabel* m_endpointHintLabel = nullptr;
    QLineEdit* m_endpointUrlEdit = nullptr;
    QLineEdit* m_siteCodeEdit = nullptr;
    QLineEdit* m_stationCodeEdit = nullptr;
    QLineEdit* m_userNameEdit = nullptr;
    QLineEdit* m_boardCodeEdit = nullptr;
    QLineEdit* m_lineNameEdit = nullptr;
    QLineEdit* m_programNameEdit = nullptr;
    QLineEdit* m_deviceNameEdit = nullptr;
    QLineEdit* m_operatorNameEdit = nullptr;
    QSpinBox* m_pointCountSpin = nullptr;
    QSpinBox* m_portSpinBox = nullptr;
    QPushButton* m_toggleReceiverButton = nullptr;
    QPlainTextEdit* m_payloadEdit = nullptr;
    QPlainTextEdit* m_logEdit = nullptr;
};

}  // namespace HostSpc
