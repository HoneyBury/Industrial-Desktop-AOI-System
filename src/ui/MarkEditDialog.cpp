#include "ui/MarkEditDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QLabel>
#include <QVBoxLayout>

MarkEditDialog::MarkEditDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Mark Editor"));
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(QStringLiteral("Dual-Mark template setup placeholder"), this));
}

#endif

