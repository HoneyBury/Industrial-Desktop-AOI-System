#pragma once

#ifdef AOI_HAS_QT_WIDGETS
#include <QDialog>

class MarkEditDialog final : public QDialog {
  Q_OBJECT

public:
  explicit MarkEditDialog(QWidget *parent = nullptr);
};
#endif

