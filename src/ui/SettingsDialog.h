#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "config/AppSettings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

class SettingsDialog final : public QDialog {
  Q_OBJECT

public:
  explicit SettingsDialog(QWidget *parent = nullptr);

  void setSettings(const AppSettings &settings);
  [[nodiscard]] AppSettings settings() const;

private:
  void buildAppConfigTab(QWidget *tab);
  void buildUiConfigTab(QWidget *tab);
  void buildRuntimeConfigTab(QWidget *tab);

  // App 配置
  QLineEdit *programOpenPathEdit_ {nullptr};
  QLineEdit *templateFolderPathEdit_ {nullptr};
  QCheckBox *persistLogsCheckBox_ {nullptr};
  QLineEdit *logFilePathEdit_ {nullptr};

  // UI 配置
  QCheckBox *showLogWindowCheckBox_ {nullptr};

  // 运行配置
  QComboBox *workModeComboBox_ {nullptr};
  QCheckBox *alarmEnabledCheckBox_ {nullptr};
  QSpinBox *maxDefectCountSpinBox_ {nullptr};
};

#endif
