#include "ui/DataCollectDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

DataCollectDialog::DataCollectDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("AI Dataset Collection"));
  resize(520, 400);

  auto *rootLayout = new QVBoxLayout(this);

  auto *headerLabel = new QLabel(
      QStringLiteral("Collect and label images for AI model training."), this);
  headerLabel->setWordWrap(true);
  rootLayout->addWidget(headerLabel);

  // Image list
  imageListWidget_ = new QListWidget(this);
  rootLayout->addWidget(imageListWidget_, 1);

  // Status bar
  auto *statusLayout = new QHBoxLayout;
  countLabel_ = new QLabel(QStringLiteral("Images: 0"), this);
  statusLayout->addWidget(countLabel_);
  statusLayout->addStretch();

  captureButton_ = new QPushButton(QStringLiteral("Capture Frame"), this);
  captureButton_->setToolTip(QStringLiteral("Capture current camera frame and add to dataset."));
  statusLayout->addWidget(captureButton_);
  rootLayout->addLayout(statusLayout);

  // Export row
  auto *exportLayout = new QHBoxLayout;
  exportLayout->addWidget(new QLabel(QStringLiteral("Export to:"), this));
  exportPathEdit_ = new QLineEdit(this);
  exportPathEdit_->setPlaceholderText(QStringLiteral("Select export directory..."));
  exportPathEdit_->setReadOnly(true);
  exportLayout->addWidget(exportPathEdit_, 1);

  auto *browseButton = new QPushButton(QStringLiteral("Browse"), this);
  exportLayout->addWidget(browseButton);

  exportButton_ = new QPushButton(QStringLiteral("Export Dataset"), this);
  exportButton_->setEnabled(false);
  exportLayout->addWidget(exportButton_);
  rootLayout->addLayout(exportLayout);

  auto *closeButton = new QPushButton(QStringLiteral("Close"), this);
  rootLayout->addWidget(closeButton);

  // Connections
  connect(captureButton_, &QPushButton::clicked, this, &DataCollectDialog::onCaptureClicked);
  connect(browseButton, &QPushButton::clicked, this, [this]() {
    const QString dir = QFileDialog::getExistingDirectory(this,
        QStringLiteral("Select Export Directory"), captureDirectory_);
    if (!dir.isEmpty()) {
      exportPathEdit_->setText(dir);
      exportButton_->setEnabled(true);
    }
  });
  connect(exportButton_, &QPushButton::clicked, this, &DataCollectDialog::onExportClicked);
  connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
}

void DataCollectDialog::setCaptureDirectory(const QString &directory) {
  captureDirectory_ = directory;
  if (exportPathEdit_->text().isEmpty()) {
    exportPathEdit_->setText(directory + QStringLiteral("/dataset_export"));
  }
  exportButton_->setEnabled(!directory.isEmpty());
}

void DataCollectDialog::addCapturedImage(const QString &imagePath, const QString &label) {
  const QString displayText = label.isEmpty()
                                  ? imagePath
                                  : QStringLiteral("[%1] %2").arg(label, imagePath);
  imageListWidget_->addItem(displayText);
  countLabel_->setText(QStringLiteral("Images: %1").arg(imageListWidget_->count()));
}

void DataCollectDialog::onCaptureClicked() {
  emit captureRequested();
}

void DataCollectDialog::onExportClicked() {
  if (imageListWidget_->count() == 0) {
    QMessageBox::information(this, QStringLiteral("No Data"),
                             QStringLiteral("No images have been collected yet."));
    return;
  }

  emit exportRequested(exportPathEdit_->text());
  QMessageBox::information(this, QStringLiteral("Export"),
                           QStringLiteral("Dataset export initiated to:\n%1").arg(exportPathEdit_->text()));
}

#endif
