#pragma once

#include <string>

#ifdef AOI_HAS_OPENCV
#include <opencv2/core/mat.hpp>
#else
// Minimal forward-declaration stub for environments without OpenCV.
namespace cv {
class Mat {};
} // namespace cv
#endif

/// 相机提供者抽象接口。
///
/// 支持两种模式：
/// - MacCameraProvider：Mac 自带摄像头实时采集
/// - DemoImageCameraProvider：大图裁剪模拟视野移动
class ICameraProvider {
public:
  virtual ~ICameraProvider() = default;

  virtual bool open() = 0;
  virtual void close() = 0;
  virtual bool isOpen() const = 0;

#ifdef AOI_HAS_OPENCV
  virtual cv::Mat capture() = 0;
#else
  virtual cv::Mat capture() = 0;
#endif

  virtual std::string providerName() const = 0;
};
