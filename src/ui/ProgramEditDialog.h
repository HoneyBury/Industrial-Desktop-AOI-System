#pragma once

#ifdef AOI_HAS_QT_WIDGETS
#include <QDialog>

class ProgramEditDialog final : public QDialog {
  Q_OBJECT

public:
  explicit ProgramEditDialog(QWidget *parent = nullptr);
};
#endif

