#pragma once

#ifdef AOI_HAS_QT_WIDGETS
#include <QDialog>

class MarkOffsetDialog final : public QDialog {
  Q_OBJECT

public:
  explicit MarkOffsetDialog(QWidget *parent = nullptr);
};
#endif

