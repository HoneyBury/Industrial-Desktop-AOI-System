#pragma once

#include "domain/Models.h"
#include "process/WorkflowContext.h"

#include <QObject>
#include <QString>

namespace HostSpc {
class SpcWriteManager;
} // namespace HostSpc

/// Converts AOI workflow laser-point results into SPC InspectionBatch records
/// and writes them through SpcWriteManager so the LaserSpc dashboard can query
/// and display production statistics.
class LaserSpcBridge final : public QObject {
  Q_OBJECT

public:
  explicit LaserSpcBridge(QObject *parent = nullptr);

  /// Attach a write manager owned by the host (MainWindow).
  void setWriteManager(HostSpc::SpcWriteManager *writer);

  /// Build an SPC batch from the given workflow context and write it
  /// synchronously.  Returns true when the database write succeeded.
  bool submitWorkflowResult(const WorkflowContext &context,
                            const QString &lineName = QStringLiteral("L1"),
                            const QString &deviceName = QStringLiteral("Laser-01"));

  /// Convenience: build an InspectionBatch (without writing).
  LaserSpc::Domain::InspectionBatch
  buildBatch(const WorkflowContext &context, const QString &lineName,
             const QString &deviceName) const;

  /// Last ingest result for UI feedback.
  [[nodiscard]] const LaserSpc::Domain::IngestResult &lastResult() const;

signals:
  void batchWritten(bool ok, const QString &summary);

private:
  HostSpc::SpcWriteManager *writer_ = nullptr;
  LaserSpc::Domain::IngestResult lastResult_;
};
