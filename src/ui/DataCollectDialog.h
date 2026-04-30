#pragma once

#ifdef AOI_HAS_QT_WIDGETS
#include <QDialog>

class DataCollectDialog final : public QDialog {
  Q_OBJECT

public:
  explicit DataCollectDialog(QWidget *parent = nullptr);
};
#endif

