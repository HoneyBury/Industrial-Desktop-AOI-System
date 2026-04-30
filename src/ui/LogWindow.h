#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include <QWidget>

class QTextEdit;

class LogWindow final : public QWidget {
  Q_OBJECT

public:
  explicit LogWindow(QWidget *parent = nullptr);

  void appendLog(const QString &message);
  void setPersistEnabled(bool enabled, const QString &filePath = {});
  [[nodiscard]] QStringList allEntries() const;

  void clear();

protected:
  void closeEvent(QCloseEvent *event) override;

signals:
  void windowClosed();

private:
  void flushToFile();

  QTextEdit *logTextEdit_ {nullptr};
  QStringList entries_;
  bool persistEnabled_ {false};
  QString persistFilePath_;
  qsizetype persistedEntryCount_ {0};
};

#endif
