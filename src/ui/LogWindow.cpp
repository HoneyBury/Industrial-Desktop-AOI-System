#include "ui/LogWindow.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QCloseEvent>
#include <QDateTime>
#include <QFile>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

LogWindow::LogWindow(QWidget *parent) : QWidget(parent, Qt::Window) {
  setWindowTitle(QStringLiteral("运行日志"));
  resize(620, 480);
  setAttribute(Qt::WA_DeleteOnClose, false);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  logTextEdit_ = new QTextEdit(this);
  logTextEdit_->setReadOnly(true);
  logTextEdit_->setStyleSheet(QStringLiteral(
      "QTextEdit { background: #0f172a; color: #e2e8f0; border: none; "
      "  font-family: \"SF Mono\", Menlo, monospace; font-size: 13px; }"));
  layout->addWidget(logTextEdit_);

  setStyleSheet(QStringLiteral("LogWindow { background: #0f172a; }"));
}

void LogWindow::appendLog(const QString &message) {
  const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
  const QString entry = QStringLiteral("[%1] %2").arg(timestamp, message);

  entries_.append(entry);
  if (logTextEdit_ != nullptr) {
    logTextEdit_->append(entry);
  }

  if (persistEnabled_ && !persistFilePath_.isEmpty()) {
    flushToFile();
  }
}

void LogWindow::setPersistEnabled(const bool enabled, const QString &filePath) {
  persistEnabled_ = enabled;
  if (!filePath.isEmpty() && persistFilePath_ != filePath) {
    persistFilePath_ = filePath;
    persistedEntryCount_ = 0;
  }
}

QStringList LogWindow::allEntries() const { return entries_; }

void LogWindow::clear() {
  entries_.clear();
  persistedEntryCount_ = 0;
  if (logTextEdit_ != nullptr) {
    logTextEdit_->clear();
  }
}

void LogWindow::closeEvent(QCloseEvent *event) {
  event->accept();
  emit windowClosed();
}

void LogWindow::flushToFile() {
  if (persistedEntryCount_ >= entries_.size()) {
    return;
  }

  QFile file(persistFilePath_);
  if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    QTextStream stream(&file);
    for (qsizetype index = persistedEntryCount_; index < entries_.size(); ++index) {
      const auto &entry = entries_.at(index);
      stream << entry << "\n";
    }
    persistedEntryCount_ = entries_.size();
  }
}

#endif
