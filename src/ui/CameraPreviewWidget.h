#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "camera/VirtualCameraDevice.h"

#include <QImage>
#include <QTimer>
#include <QWidget>

#include <functional>

class QLabel;

/// 实时相机预览控件，绘制十字准星和状态叠加信息
class CameraPreviewWidget final : public QWidget {
  Q_OBJECT

public:
  using FrameProvider = std::function<QImage()>;
  using PoseInfoProvider = std::function<VirtualCameraPoseInfo()>;

  explicit CameraPreviewWidget(QWidget *parent = nullptr);

  void setFrameProvider(FrameProvider provider);
  void setPoseInfoProvider(PoseInfoProvider provider);
  void startRefresh(int intervalMs = 50);
  void stopRefresh();

signals:
  void frameUpdated(const QImage &frame);
  void positionInfoChanged(const QString &info);

private:
  void refreshFrame();
  void paintOverlay(QPainter &painter, const QImage &frame);
  void updateInfo();

  QLabel *previewLabel_ {nullptr};
  QLabel *infoLabel_ {nullptr};
  QTimer *refreshTimer_ {nullptr};
  FrameProvider frameProvider_;
  PoseInfoProvider poseInfoProvider_;
  QImage currentFrame_;
};

#endif
