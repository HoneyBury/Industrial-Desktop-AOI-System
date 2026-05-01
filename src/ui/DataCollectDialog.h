#pragma once

#ifdef AOI_HAS_QT_WIDGETS
#include <QDialog>
#include <QStringList>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class DataCollectDialog final : public QDialog {
  Q_OBJECT

public:
  explicit DataCollectDialog(QWidget *parent = nullptr);

  void setCaptureDirectory(const QString &directory);
  void addCapturedImage(const QString &imagePath, const QString &label);

signals:
  void captureRequested();
  void exportRequested(const QString &exportPath);

private:
  void onCaptureClicked();
  void onExportClicked();

  QListWidget *imageListWidget_ {nullptr};
  QLabel *countLabel_ {nullptr};
  QLineEdit *exportPathEdit_ {nullptr};
  QPushButton *captureButton_ {nullptr};
  QPushButton *exportButton_ {nullptr};
  QString captureDirectory_;
};
#endif
