#include "app/IngestService.h"

#include <QDateTime>
#include <QObject>

namespace LaserSpc::App {

IngestService::IngestService(std::shared_ptr<LaserSpc::Domain::ISpcWriteRepository> repository)
    : m_repository(std::move(repository)) {}

LaserSpc::Domain::IngestResult IngestService::ingestBoardRecord(LaserSpc::Domain::BoardRecordRow row) const {
    LaserSpc::Domain::IngestResult result;
    if (!normalizeBoard(row, &result.errorMessage)) {
        return result;
    }

    result.success = m_repository->upsertBoardRecord(row);
    result.insertedBoards = result.success ? 1 : 0;
    if (!result.success) {
        result.errorMessage = m_repository->lastError();
    }
    return result;
}

LaserSpc::Domain::IngestResult IngestService::ingestPointRecord(LaserSpc::Domain::PointRecordRow row) const {
    LaserSpc::Domain::IngestResult result;
    if (!normalizePoint(row, &result.errorMessage)) {
        return result;
    }

    result.success = m_repository->insertPointRecord(row);
    result.insertedPoints = result.success ? 1 : 0;
    if (!result.success) {
        result.errorMessage = m_repository->lastError();
    }
    return result;
}

LaserSpc::Domain::IngestResult IngestService::ingestInspectionBatch(LaserSpc::Domain::InspectionBatch batch) const {
    LaserSpc::Domain::IngestResult result;
    if (!normalizeBoard(batch.board, &result.errorMessage)) {
        return result;
    }

    for (auto& point : batch.points) {
        if (point.boardCode.trimmed().isEmpty()) {
            point.boardCode = batch.board.boardCode;
        }
        if (point.lineName.trimmed().isEmpty()) {
            point.lineName = batch.board.lineName;
        }
        if (point.programName.trimmed().isEmpty()) {
            point.programName = batch.board.programName;
        }
        if (point.deviceName.trimmed().isEmpty()) {
            point.deviceName = batch.board.deviceName;
        }
        if (point.boardCode != batch.board.boardCode) {
            result.errorMessage = QObject::tr("Point record boardCode does not match batch boardCode.");
            return result;
        }
        if (!normalizePoint(point, &result.errorMessage)) {
            return result;
        }
    }

    result.success = m_repository->replaceInspectionBatch(batch.board, batch.points);
    if (result.success) {
        result.insertedBoards = 1;
        result.insertedPoints = batch.points.size();
        if (m_mesForwarder) {
            QString forwardError;
            if (!m_mesForwarder(batch, &forwardError)) {
                result.forwardMessage = forwardError;
            } else {
                result.forwardMessage = QObject::tr("MES forwarded.");
            }
        }
    } else {
        result.errorMessage = m_repository->lastError();
    }
    return result;
}

void IngestService::setMesForwarder(std::function<bool(const LaserSpc::Domain::InspectionBatch&, QString*)> forwarder) {
    m_mesForwarder = std::move(forwarder);
}

bool IngestService::normalizeBoard(LaserSpc::Domain::BoardRecordRow& row, QString* errorMessage) const {
    row.boardCode = row.boardCode.trimmed();
    row.result = row.result.trimmed().toUpper();
    row.lineName = row.lineName.trimmed();
    row.programName = row.programName.trimmed();
    row.deviceName = row.deviceName.trimmed();
    row.operatorName = row.operatorName.trimmed();
    if (!row.eventTime.isValid()) {
        row.eventTime = QDateTime::currentDateTime();
    } else {
        row.eventTime = row.eventTime.toLocalTime();
    }

    if (row.boardCode.isEmpty() || row.result.isEmpty() || row.lineName.isEmpty() || row.programName.isEmpty() ||
        row.deviceName.isEmpty() || row.operatorName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Board record requires boardCode, result, lineName, programName, deviceName and operatorName.");
        }
        return false;
    }

    if (row.result != QObject::tr("OK") && row.result != QObject::tr("NG")) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Board record result must be OK or NG.");
        }
        return false;
    }
    return true;
}

bool IngestService::normalizePoint(LaserSpc::Domain::PointRecordRow& row, QString* errorMessage) const {
    row.boardCode = row.boardCode.trimmed();
    row.pointName = row.pointName.trimmed();
    row.result = row.result.trimmed().toUpper();
    row.readGrade = row.readGrade.trimmed();
    row.laserContent = row.laserContent.trimmed();
    row.readCodeContent = row.readCodeContent.trimmed();
    if (row.readGrade.isEmpty()) {
        row.readGrade = QStringLiteral("null");
    }
    if (row.laserContent.isEmpty()) {
        row.laserContent = QStringLiteral("%1-LASER").arg(row.pointName);
    }
    if (row.readCodeContent.isEmpty()) {
        row.readCodeContent = QStringLiteral("null");
        row.isReadCode = false;
    } else if (row.readCodeContent != QStringLiteral("null")) {
        row.isReadCode = true;
    }
    row.lineName = row.lineName.trimmed();
    row.programName = row.programName.trimmed();
    row.deviceName = row.deviceName.trimmed();
    row.detailJsonPath = row.detailJsonPath.trimmed();
    if (!row.endTime.isValid()) {
        row.endTime = QDateTime::currentDateTime();
    } else {
        row.endTime = row.endTime.toLocalTime();
    }
    if (!row.startTime.isValid()) {
        row.startTime = row.endTime;
    } else {
        row.startTime = row.startTime.toLocalTime();
    }

    if (row.boardCode.isEmpty() || row.pointName.isEmpty() || row.result.isEmpty() || row.lineName.isEmpty() ||
        row.programName.isEmpty() || row.deviceName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Point record requires boardCode, pointName, result, lineName, programName and deviceName.");
        }
        return false;
    }

    if (row.result != QObject::tr("OK") && row.result != QObject::tr("NG")) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Point record result must be OK or NG.");
        }
        return false;
    }

    if (row.startTime > row.endTime) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Point record startTime must not be later than endTime.");
        }
        return false;
    }
    return true;
}

}  // namespace LaserSpc::App
