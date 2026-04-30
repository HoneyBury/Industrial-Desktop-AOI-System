#include "ui/ProgramEditDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QLabel>
#include <QVBoxLayout>

ProgramEditDialog::ProgramEditDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Program Editor"));
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(QStringLiteral("Program definition editor placeholder"), this));
}

#endif

