#include "spc/LaserSpcBridge.h"

#include "domain/Models.h"
#include "SpcWriteManager.h"

#include <QDateTime>

namespace {

LaserSpc::Domain::BoardRecordRow
buildBoardRow(const WorkflowContext &context, const QString &lineName,
              const QString &deviceName) {
  LaserSpc::Domain::BoardRecordRow row;
  row.boardCode = QString::fromStdString(context.boardId);
  row.result = context.finalDecisionOk ? QStringLiteral("OK")
                                       : QStringLiteral("NG");
  row.lineName = lineName;
  row.programName = context.program != nullptr
                        ? QString::fromStdString(context.program->name)
                        : QString();
  row.deviceName = deviceName;
  row.operatorName = QStringLiteral("AOI-System");
  row.eventTime = QDateTime::currentDateTime();
  return row;
}

LaserSpc::Domain::PointRecordRow
buildPointRow(const LaserPointExecutionResult &laserPoint,
              const QString &boardCode, const QString &lineName,
              const QString &programName, const QString &deviceName) {
  LaserSpc::Domain::PointRecordRow row;
  row.boardCode = boardCode;
  row.pointName = QString::fromStdString(laserPoint.taskName);
  row.result = laserPoint.passed ? QStringLiteral("OK") : QStringLiteral("NG");
  row.readGrade = laserPoint.codeVerified ? QStringLiteral("A")
                                          : QStringLiteral("C");
  row.laserContent = QString::fromStdString(laserPoint.expectedCodeText);
  row.readCodeContent = QString::fromStdString(laserPoint.decodedText);
  row.isLaser = true;
  row.isReadCode = true;
  row.lineName = lineName;
  row.programName = programName;
  row.deviceName = deviceName;
  row.startTime = QDateTime::currentDateTime().addSecs(-2);
  row.endTime = QDateTime::currentDateTime();

  // Populate detail info for the SPC point-detail dialog.
  row.detail.laserContent = row.laserContent;
  row.detail.readCodeContent = row.readCodeContent;
  row.detail.success = laserPoint.passed;
  row.detail.programName = programName;
  row.detail.startTime = row.startTime;
  row.detail.endTime = row.endTime;

  // Extra fields visible in the detail panel.
  row.detail.extraFields.insert(
      QStringLiteral("machineX"), laserPoint.machinePose.x);
  row.detail.extraFields.insert(
      QStringLiteral("machineY"), laserPoint.machinePose.y);
  row.detail.extraFields.insert(
      QStringLiteral("machineZ"), laserPoint.machinePose.z);
  row.detail.extraFields.insert(
      QStringLiteral("machineR"), laserPoint.machinePose.r);
  row.detail.extraFields.insert(
      QStringLiteral("productX"), laserPoint.productPoint.x);
  row.detail.extraFields.insert(
      QStringLiteral("productY"), laserPoint.productPoint.y);

  row.detail.fieldDisplayNames.insert(QStringLiteral("machineX"),
                                      QString::fromUtf8("机械X"));
  row.detail.fieldDisplayNames.insert(QStringLiteral("machineY"),
                                      QString::fromUtf8("机械Y"));
  row.detail.fieldDisplayNames.insert(QStringLiteral("machineZ"),
                                      QString::fromUtf8("机械Z"));
  row.detail.fieldDisplayNames.insert(QStringLiteral("machineR"),
                                      QString::fromUtf8("机械R"));
  row.detail.fieldDisplayNames.insert(QStringLiteral("productX"),
                                      QString::fromUtf8("产品X(mm)"));
  row.detail.fieldDisplayNames.insert(QStringLiteral("productY"),
                                      QString::fromUtf8("产品Y(mm)"));

  // Algorithm plan: the 3-step flow (position → mark → verify).
  QJsonArray plan;
  plan.append(QJsonObject{{QStringLiteral("name"), QString::fromUtf8("定位")},
                          {QStringLiteral("ok"), QString::fromUtf8("进入镭雕")},
                          {QStringLiteral("ng"), QString::fromUtf8("输出定位失败")}});
  plan.append(QJsonObject{{QStringLiteral("name"), QString::fromUtf8("镭雕")},
                          {QStringLiteral("ok"), QString::fromUtf8("进入读码")},
                          {QStringLiteral("ng"), QString::fromUtf8("输出镭雕失败")}});
  plan.append(QJsonObject{{QStringLiteral("name"), QString::fromUtf8("读码验证")},
                          {QStringLiteral("ok"), QString::fromUtf8("判定OK")},
                          {QStringLiteral("ng"), QString::fromUtf8("判定NG")}});
  row.detail.algorithmPlan = plan;
  row.detail.fieldDisplayNames.insert(QStringLiteral("name"),
                                      QString::fromUtf8("节点名称"));
  row.detail.fieldDisplayNames.insert(QStringLiteral("ok"),
                                      QString::fromUtf8("成功分支"));
  row.detail.fieldDisplayNames.insert(QStringLiteral("ng"),
                                      QString::fromUtf8("失败分支"));

  return row;
}

} // namespace

LaserSpcBridge::LaserSpcBridge(QObject *parent) : QObject(parent) {}

void LaserSpcBridge::setWriteManager(HostSpc::SpcWriteManager *writer) {
  writer_ = writer;
}

LaserSpc::Domain::InspectionBatch
LaserSpcBridge::buildBatch(const WorkflowContext &context,
                           const QString &lineName,
                           const QString &deviceName) const {
  const auto boardRow = buildBoardRow(context, lineName, deviceName);
  const QString boardCode = boardRow.boardCode;
  const QString programName = boardRow.programName;

  QList<LaserSpc::Domain::PointRecordRow> points;
  points.reserve(static_cast<int>(context.laserPointResults.size()));
  for (const auto &lp : context.laserPointResults) {
    points.append(
        buildPointRow(lp, boardCode, lineName, programName, deviceName));
  }

  return HostSpc::SpcWriteManager::buildBatch(QString(), boardRow, points);
}

bool LaserSpcBridge::submitWorkflowResult(const WorkflowContext &context,
                                          const QString &lineName,
                                          const QString &deviceName) {
  if (writer_ == nullptr) {
    lastResult_ = {};
    lastResult_.success = false;
    lastResult_.errorMessage =
        QStringLiteral("SpcWriteManager is not set; cannot submit SPC data.");
    emit batchWritten(false, lastResult_.errorMessage);
    return false;
  }

  if (context.laserPointResults.empty()) {
    lastResult_ = {};
    lastResult_.success = false;
    lastResult_.errorMessage = QStringLiteral(
        "No laser point results in workflow context; nothing to submit.");
    emit batchWritten(false, lastResult_.errorMessage);
    return false;
  }

  const auto batch = buildBatch(context, lineName, deviceName);
  lastResult_ = writer_->storeBatch(batch);

  const QString summary =
      lastResult_.success
          ? QStringLiteral("SPC batch written: board=%1 points=%2")
                .arg(lastResult_.insertedBoards)
                .arg(lastResult_.insertedPoints)
          : QStringLiteral("SPC write failed: %1").arg(lastResult_.errorMessage);

  emit batchWritten(lastResult_.success, summary);
  return lastResult_.success;
}

const LaserSpc::Domain::IngestResult &LaserSpcBridge::lastResult() const {
  return lastResult_;
}
