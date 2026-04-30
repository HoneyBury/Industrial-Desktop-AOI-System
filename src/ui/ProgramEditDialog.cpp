#include "ui/ProgramEditDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "program/ProgramManager.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QImage buildProgramPreviewImage(const ProgramModel &program) {
  QImage image(640, 420, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor("#0b1220"));

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.fillRect(QRect(18, 18, 604, 384), QColor("#111827"));
  painter.setPen(QPen(QColor("#233250"), 1));

  for (int x = 18; x <= 622; x += 40) {
    painter.drawLine(x, 18, x, 402);
  }

  for (int y = 18; y <= 402; y += 40) {
    painter.drawLine(18, y, 622, y);
  }

  painter.setPen(QPen(QColor("#facc15"), 3, Qt::DashLine));
  painter.drawRect(QRect(140, 96, 240, 140));
  painter.drawText(QRect(150, 104, 200, 28), QStringLiteral("实时 FOV 范围"));

  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#ef4444"));
  for (const auto &mark : program.marks) {
    painter.drawEllipse(QPointF(80.0 + mark.x * 1.6, 70.0 + mark.y * 1.4), 8.0, 8.0);
  }

  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(QColor("#22c55e"), 3));
  for (const auto &roi : program.rois) {
    painter.drawRect(
        QRectF(80.0 + roi.x * 1.6, 70.0 + roi.y * 1.4, roi.width * 1.8, roi.height * 1.8));
  }

  painter.setPen(QColor("#cbd5e1"));
  painter.drawText(QRect(24, 360, 590, 28),
                   QStringLiteral("程序：%1 | Mark：%2 | ROI：%3")
                       .arg(QString::fromStdString(program.name))
                       .arg(static_cast<int>(program.marks.size()))
                       .arg(static_cast<int>(program.rois.size())));
  return image;
}

} // namespace

ProgramEditDialog::ProgramEditDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("打开程序"));
  resize(760, 620);

  auto *rootLayout = new QVBoxLayout(this);

  auto *fileGroupBox = new QGroupBox(QStringLiteral("程序文件"), this);
  auto *fileLayout = new QHBoxLayout(fileGroupBox);
  filePathLineEdit_ = new QLineEdit(fileGroupBox);
  filePathLineEdit_->setReadOnly(true);
  auto *browseButton = new QPushButton(QStringLiteral("浏览..."), fileGroupBox);
  auto *defaultButton = new QPushButton(QStringLiteral("使用默认程序"), fileGroupBox);
  fileLayout->addWidget(filePathLineEdit_, 1);
  fileLayout->addWidget(browseButton);
  fileLayout->addWidget(defaultButton);
  rootLayout->addWidget(fileGroupBox);

  auto *contentLayout = new QHBoxLayout;
  rootLayout->addLayout(contentLayout, 1);

  auto *detailGroupBox = new QGroupBox(QStringLiteral("程序详情"), this);
  auto *detailFormLayout = new QFormLayout(detailGroupBox);
  programNameValueLabel_ = new QLabel(QStringLiteral("--"), detailGroupBox);
  programAiModelValueLabel_ = new QLabel(QStringLiteral("--"), detailGroupBox);
  programMarksValueLabel_ = new QLabel(QStringLiteral("0"), detailGroupBox);
  programRoisValueLabel_ = new QLabel(QStringLiteral("0"), detailGroupBox);
  statusLabel_ = new QLabel(QStringLiteral("请先选择一个程序文件。"), detailGroupBox);
  statusLabel_->setWordWrap(true);
  detailFormLayout->addRow(QStringLiteral("程序名称"), programNameValueLabel_);
  detailFormLayout->addRow(QStringLiteral("AI 模型"), programAiModelValueLabel_);
  detailFormLayout->addRow(QStringLiteral("Mark 数量"), programMarksValueLabel_);
  detailFormLayout->addRow(QStringLiteral("ROI 数量"), programRoisValueLabel_);
  detailFormLayout->addRow(QStringLiteral("加载状态"), statusLabel_);
  contentLayout->addWidget(detailGroupBox, 1);

  auto *previewGroupBox = new QGroupBox(QStringLiteral("FOV 拼接预览"), this);
  auto *previewLayout = new QVBoxLayout(previewGroupBox);
  previewLabel_ = new QLabel(QStringLiteral("尚未生成程序预览"), previewGroupBox);
  previewLabel_->setMinimumSize(420, 320);
  previewLabel_->setAlignment(Qt::AlignCenter);
  previewLabel_->setWordWrap(true);
  previewLabel_->setStyleSheet(QStringLiteral(
      "background: #0f172a; color: #e2e8f0; border: 1px solid #334155; border-radius: 10px;"));
  previewLayout->addWidget(previewLabel_, 1);
  contentLayout->addWidget(previewGroupBox, 2);

  buttonBox_ =
      new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, this);
  connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
  rootLayout->addWidget(buttonBox_);

  connect(browseButton, &QPushButton::clicked, this, &ProgramEditDialog::browseProgramFile);
  connect(defaultButton, &QPushButton::clicked, this, &ProgramEditDialog::useDefaultProgramFile);

  updateAcceptState();
}

void ProgramEditDialog::setProjectRootPath(const QString &projectRootPath) { projectRootPath_ = projectRootPath; }

void ProgramEditDialog::setSelectedFilePath(const QString &filePath) {
  if (!filePath.isEmpty()) {
    loadProgramFile(filePath);
  }
}

QString ProgramEditDialog::selectedFilePath() const { return selectedFilePath_; }

void ProgramEditDialog::browseProgramFile() {
  const QString initialDirectory =
      projectRootPath_.isEmpty() ? QDir::homePath() : QDir(projectRootPath_).filePath(QStringLiteral("config"));
  const QString filePath = QFileDialog::getOpenFileName(
      this, QStringLiteral("选择程序文件"), initialDirectory,
      QStringLiteral("Program Files (*.json);;All Files (*)"));

  if (!filePath.isEmpty()) {
    loadProgramFile(filePath);
  }
}

void ProgramEditDialog::useDefaultProgramFile() {
  if (projectRootPath_.isEmpty()) {
    return;
  }

  loadProgramFile(QDir(projectRootPath_).filePath(QStringLiteral("config/default_program.json")));
}

void ProgramEditDialog::loadProgramFile(const QString &filePath) {
  ProgramManager manager;
  const auto result = manager.loadProgram(filePath.toStdString());

  if (!result) {
    selectedFilePath_.clear();
    filePathLineEdit_->setText(filePath);
    programNameValueLabel_->setText(QStringLiteral("--"));
    programAiModelValueLabel_->setText(QStringLiteral("--"));
    programMarksValueLabel_->setText(QStringLiteral("0"));
    programRoisValueLabel_->setText(QStringLiteral("0"));
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText(QStringLiteral("程序加载失败"));
    statusLabel_->setText(QString::fromStdString(result.message));
    updateAcceptState();
    return;
  }

  selectedFilePath_ = filePath;
  filePathLineEdit_->setText(filePath);
  programNameValueLabel_->setText(QString::fromStdString(result.value.name));
  programAiModelValueLabel_->setText(QString::fromStdString(result.value.aiModelPath));
  programMarksValueLabel_->setText(QString::number(result.value.marks.size()));
  programRoisValueLabel_->setText(QString::number(result.value.rois.size()));
  previewLabel_->setText(QString());
  previewLabel_->setPixmap(QPixmap::fromImage(buildProgramPreviewImage(result.value)).scaled(
      previewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  statusLabel_->setText(QStringLiteral("已成功解析程序文件，可用于打开。"));
  updateAcceptState();
}

void ProgramEditDialog::updateAcceptState() {
  if (auto *openButton = buttonBox_->button(QDialogButtonBox::Open); openButton != nullptr) {
    openButton->setEnabled(!selectedFilePath_.isEmpty());
  }
}

#endif
