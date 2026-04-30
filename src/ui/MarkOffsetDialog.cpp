#include "ui/MarkOffsetDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QLabel>
#include <QVBoxLayout>

MarkOffsetDialog::MarkOffsetDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Mark Offset Calibration"));
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(QStringLiteral("Mark offset correction placeholder"), this));
}

#endif

