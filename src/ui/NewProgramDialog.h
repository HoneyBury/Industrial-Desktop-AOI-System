#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "program/ProgramModel.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;

class NewProgramDialog final : public QDialog {
  Q_OBJECT

public:
  explicit NewProgramDialog(QWidget *parent = nullptr);

  void setInitialProgramName(const QString &programName);
  void setInitialBoardDefinition(const BoardDefinition &boardDefinition);
  void setInitialScanRecipe(const ScanRecipe &scanRecipe);

  [[nodiscard]] QString programName() const;
  [[nodiscard]] BoardDefinition boardDefinition() const;
  [[nodiscard]] ScanRecipe scanRecipe() const;

private:
  QLineEdit *programNameLineEdit_ {nullptr};
  QDoubleSpinBox *boardLengthSpinBox_ {nullptr};
  QDoubleSpinBox *boardWidthSpinBox_ {nullptr};
  QDoubleSpinBox *railWidthSpinBox_ {nullptr};
  QDoubleSpinBox *fovWidthSpinBox_ {nullptr};
  QDoubleSpinBox *fovHeightSpinBox_ {nullptr};
  QComboBox *scanOrderComboBox_ {nullptr};
};

#endif
