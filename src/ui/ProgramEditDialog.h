#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;
class QDialogButtonBox;

class ProgramEditDialog final : public QDialog {
  Q_OBJECT

public:
  explicit ProgramEditDialog(QWidget *parent = nullptr);

  void setProjectRootPath(const QString &projectRootPath);
  void setSelectedFilePath(const QString &filePath);
  [[nodiscard]] QString selectedFilePath() const;

private:
  void browseProgramFile();
  void useDefaultProgramFile();
  void loadProgramFile(const QString &filePath);
  void updateAcceptState();

  QString projectRootPath_;
  QString selectedFilePath_;
  QLineEdit *filePathLineEdit_ {nullptr};
  QLabel *programNameValueLabel_ {nullptr};
  QLabel *programAiModelValueLabel_ {nullptr};
  QLabel *programMarksValueLabel_ {nullptr};
  QLabel *programRoisValueLabel_ {nullptr};
  QLabel *previewLabel_ {nullptr};
  QLabel *statusLabel_ {nullptr};
  QDialogButtonBox *buttonBox_ {nullptr};
};

#endif
