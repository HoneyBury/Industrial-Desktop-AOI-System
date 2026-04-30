#include "ui/DataCollectDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QLabel>
#include <QVBoxLayout>

DataCollectDialog::DataCollectDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("AI Dataset Collection"));
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(QStringLiteral("Dataset capture and export placeholder"), this));
}

#endif

