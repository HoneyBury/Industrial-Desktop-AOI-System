#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include <QImage>
#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;
class QTimer;

class CalibrationCaptureWidget final : public QWidget {
  Q_OBJECT

public:
  using FrameProvider = std::function<QImage()>;
  using CameraRunningProvider = std::function<bool()>;

  explicit CalibrationCaptureWidget(QWidget *parent = nullptr);

  void setPanelTitle(const QString &title);
  void setInstructionText(const QString &text);
  void setFrameProvider(FrameProvider frameProvider, CameraRunningProvider runningProvider);
  [[nodiscard]] QImage capturedFrame() const;
  [[nodiscard]] bool hasCapturedFrame() const;
  void clearCapturedFrame();

signals:
  void frameAccepted(const QImage &image);
  void frozenStateChanged(bool frozen);

private:
  void refreshPreview();
  void updatePreviewPixmap();
  void setFrozen(bool frozen);

  QLabel *titleLabel_ {nullptr};
  QLabel *instructionLabel_ {nullptr};
  QLabel *statusLabel_ {nullptr};
  QLabel *previewLabel_ {nullptr};
  QPushButton *freezeButton_ {nullptr};
  QPushButton *resumeButton_ {nullptr};
  QPushButton *useFrameButton_ {nullptr};
  QTimer *refreshTimer_ {nullptr};
  FrameProvider frameProvider_;
  CameraRunningProvider runningProvider_;
  QImage liveFrame_;
  QImage capturedFrame_;
  bool frozen_ {false};
};

#endif
