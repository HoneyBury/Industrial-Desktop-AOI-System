#include "ui/SettingsDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

constexpr int kMinFieldHeight = 30;

QLineEdit *makePathEdit(QWidget *parent, const QString &placeholder) {
  auto *edit = new QLineEdit(parent);
  edit->setPlaceholderText(placeholder);
  edit->setMinimumHeight(kMinFieldHeight);
  edit->setStyleSheet(QStringLiteral(
      "QLineEdit { background: #1e293b; color: #e2e8f0; border: 1px solid #334155; "
      "  border-radius: 6px; padding: 5px 10px; }"));
  return edit;
}

QString tabStyleSheet() {
  return QStringLiteral(
      "QTabWidget::pane { border: 1px solid #334155; background: #0f172a; border-radius: 8px; }"
      "QTabBar::tab { background: #1e293b; color: #94a3b8; border: 1px solid #334155; "
      "  padding: 8px 20px; margin-right: 2px; border-radius: 6px 6px 0 0; }"
      "QTabBar::tab:selected { background: #0f172a; color: #e2e8f0; border-bottom: 2px solid #2563eb; }"
      "QTabBar::tab:hover { background: #162033; color: #cbd5e1; }"
      "QGroupBox { color: #cbd5e1; font-weight: 600; border: 1px solid #334155; border-radius: 8px; "
      "  margin-top: 10px; padding-top: 14px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
      "QLabel { color: #cbd5e1; }"
      "QCheckBox { color: #e2e8f0; }"
      "QComboBox { background: #1e293b; color: #e2e8f0; border: 1px solid #334155; "
      "  border-radius: 6px; padding: 5px 10px; min-height: 28px; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox QAbstractItemView { background: #1e293b; color: #e2e8f0; "
      "  selection-background-color: #2563eb; }"
      "QSpinBox { background: #1e293b; color: #e2e8f0; border: 1px solid #334155; "
      "  border-radius: 6px; padding: 5px 10px; min-height: 28px; }");
}

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("系统设置"));
  resize(620, 520);
  setStyleSheet(QStringLiteral("QDialog { background: #0f172a; }"));

  auto *rootLayout = new QVBoxLayout(this);

  auto *tabWidget = new QTabWidget(this);
  tabWidget->setStyleSheet(tabStyleSheet());

  auto *appTab = new QWidget(tabWidget);
  auto *uiTab = new QWidget(tabWidget);
  auto *runtimeTab = new QWidget(tabWidget);

  buildAppConfigTab(appTab);
  buildUiConfigTab(uiTab);
  buildRuntimeConfigTab(runtimeTab);

  tabWidget->addTab(appTab, QStringLiteral("应用配置"));
  tabWidget->addTab(uiTab, QStringLiteral("UI 配置"));
  tabWidget->addTab(runtimeTab, QStringLiteral("运行配置"));

  rootLayout->addWidget(tabWidget);

  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttonBox->setStyleSheet(QStringLiteral(
      "QPushButton { background: #1e3a5f; color: #e2e8f0; border: 1px solid #334155; "
      "  border-radius: 8px; padding: 8px 20px; min-height: 28px; }"
      "QPushButton:hover { background: #2563eb; }"));
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  rootLayout->addWidget(buttonBox);
}

void SettingsDialog::buildAppConfigTab(QWidget *tab) {
  auto *layout = new QVBoxLayout(tab);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  auto *pathGroup = new QGroupBox(QStringLiteral("路径配置"), tab);
  auto *pathLayout = new QFormLayout(pathGroup);
  pathLayout->setSpacing(10);

  programOpenPathEdit_ = makePathEdit(pathGroup, QStringLiteral("程序文件默认打开目录"));
  templateFolderPathEdit_ = makePathEdit(pathGroup, QStringLiteral("模板文件夹路径"));

  auto makeBrowseRow = [tab](QLineEdit *edit) {
    auto *row = new QHBoxLayout;
    row->setSpacing(6);
    auto *browseButton = new QPushButton(QStringLiteral("浏览..."), tab);
    browseButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1e3a5f; color: #e2e8f0; border: 1px solid #334155; "
        "  border-radius: 6px; padding: 5px 14px; }"
        "QPushButton:hover { background: #2563eb; }"));
    QObject::connect(browseButton, &QPushButton::clicked, [edit] {
      const QString dir = QFileDialog::getExistingDirectory(edit->parentWidget(), QStringLiteral("选择目录"));
      if (!dir.isEmpty()) {
        edit->setText(dir);
      }
    });
    row->addWidget(edit, 1);
    row->addWidget(browseButton);
    return row;
  };

  pathLayout->addRow(QStringLiteral("程序路径"), makeBrowseRow(programOpenPathEdit_));
  pathLayout->addRow(QStringLiteral("模板路径"), makeBrowseRow(templateFolderPathEdit_));
  layout->addWidget(pathGroup);

  auto *logGroup = new QGroupBox(QStringLiteral("日志模块"), tab);
  auto *logLayout = new QVBoxLayout(logGroup);
  logLayout->setSpacing(8);

  persistLogsCheckBox_ = new QCheckBox(QStringLiteral("持久化保存日志到文件"), logGroup);
  persistLogsCheckBox_->setStyleSheet(QStringLiteral("QCheckBox { color: #e2e8f0; }"));

  auto *logPathRow = new QHBoxLayout;
  logPathRow->setSpacing(6);
  logFilePathEdit_ = makePathEdit(logGroup, QStringLiteral("日志文件保存路径"));
  auto *logBrowseButton = new QPushButton(QStringLiteral("浏览..."), logGroup);
  logBrowseButton->setStyleSheet(QStringLiteral(
      "QPushButton { background: #1e3a5f; color: #e2e8f0; border: 1px solid #334155; "
      "  border-radius: 6px; padding: 5px 14px; }"
      "QPushButton:hover { background: #2563eb; }"));
  QObject::connect(logBrowseButton, &QPushButton::clicked, [this] {
    const QString filePath = QFileDialog::getSaveFileName(this, QStringLiteral("选择日志文件"), {},
                                                          QStringLiteral("Log Files (*.log);;All Files (*)"));
    if (!filePath.isEmpty()) {
      logFilePathEdit_->setText(filePath);
    }
  });
  logPathRow->addWidget(logFilePathEdit_, 1);
  logPathRow->addWidget(logBrowseButton);

  logLayout->addWidget(persistLogsCheckBox_);
  logLayout->addLayout(logPathRow);
  layout->addWidget(logGroup);

  layout->addStretch();
}

void SettingsDialog::buildUiConfigTab(QWidget *tab) {
  auto *layout = new QVBoxLayout(tab);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  auto *windowGroup = new QGroupBox(QStringLiteral("独立窗口显示"), tab);
  auto *windowLayout = new QVBoxLayout(windowGroup);
  windowLayout->setSpacing(8);

  showLogWindowCheckBox_ = new QCheckBox(QStringLiteral("启动时自动打开运行日志窗口"), windowGroup);
  showLogWindowCheckBox_->setStyleSheet(QStringLiteral("QCheckBox { color: #e2e8f0; }"));
  windowLayout->addWidget(showLogWindowCheckBox_);

  auto *hintLabel = new QLabel(QStringLiteral("日志窗口也可以从菜单栏「视图 → 运行日志」手动打开/关闭。"), windowGroup);
  hintLabel->setWordWrap(true);
  hintLabel->setStyleSheet(QStringLiteral("color: #64748b; font-size: 12px;"));
  windowLayout->addWidget(hintLabel);

  layout->addWidget(windowGroup);
  layout->addStretch();
}

void SettingsDialog::buildRuntimeConfigTab(QWidget *tab) {
  auto *layout = new QVBoxLayout(tab);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  auto *runtimeGroup = new QGroupBox(QStringLiteral("运行参数"), tab);
  auto *runtimeLayout = new QFormLayout(runtimeGroup);
  runtimeLayout->setSpacing(10);

  workModeComboBox_ = new QComboBox(runtimeGroup);
  workModeComboBox_->addItems({QStringLiteral("手动模式"), QStringLiteral("自动模式")});

  alarmEnabledCheckBox_ = new QCheckBox(QStringLiteral("启用报警"), runtimeGroup);
  alarmEnabledCheckBox_->setStyleSheet(QStringLiteral("QCheckBox { color: #e2e8f0; }"));

  maxDefectCountSpinBox_ = new QSpinBox(runtimeGroup);
  maxDefectCountSpinBox_->setRange(1, 9999);
  maxDefectCountSpinBox_->setValue(10);
  maxDefectCountSpinBox_->setSuffix(QStringLiteral(" 个"));

  runtimeLayout->addRow(QStringLiteral("工作模式"), workModeComboBox_);
  runtimeLayout->addRow(QStringLiteral("报警开关"), alarmEnabledCheckBox_);
  runtimeLayout->addRow(QStringLiteral("缺陷上限"), maxDefectCountSpinBox_);
  layout->addWidget(runtimeGroup);

  auto *statsGroup = new QGroupBox(QStringLiteral("任务统计（运行后生效）"), tab);
  auto *statsLayout = new QVBoxLayout(statsGroup);
  auto *statsLabel = new QLabel(QStringLiteral("进入运行界面后，系统将自动统计检测总数、通过率、缺陷分布等数据。"), statsGroup);
  statsLabel->setWordWrap(true);
  statsLabel->setStyleSheet(QStringLiteral("color: #64748b; font-size: 12px;"));
  statsLayout->addWidget(statsLabel);
  layout->addWidget(statsGroup);

  layout->addStretch();
}

void SettingsDialog::setSettings(const AppSettings &settings) {
  programOpenPathEdit_->setText(QString::fromStdString(settings.programOpenPath));
  templateFolderPathEdit_->setText(QString::fromStdString(settings.templateFolderPath));
  persistLogsCheckBox_->setChecked(settings.persistLogs);
  logFilePathEdit_->setText(QString::fromStdString(settings.logFilePath));
  showLogWindowCheckBox_->setChecked(settings.showLogWindow);
  workModeComboBox_->setCurrentText(settings.workMode == "auto" ? QStringLiteral("自动模式")
                                                                 : QStringLiteral("手动模式"));
  alarmEnabledCheckBox_->setChecked(settings.alarmEnabled);
  maxDefectCountSpinBox_->setValue(settings.maxDefectCount);
}

AppSettings SettingsDialog::settings() const {
  AppSettings s;
  s.programOpenPath = programOpenPathEdit_->text().toStdString();
  s.templateFolderPath = templateFolderPathEdit_->text().toStdString();
  s.persistLogs = persistLogsCheckBox_->isChecked();
  s.logFilePath = logFilePathEdit_->text().toStdString();
  s.showLogWindow = showLogWindowCheckBox_->isChecked();
  s.workMode = workModeComboBox_->currentIndex() == 1 ? "auto" : "manual";
  s.alarmEnabled = alarmEnabledCheckBox_->isChecked();
  s.maxDefectCount = maxDefectCountSpinBox_->value();
  return s;
}

#endif
