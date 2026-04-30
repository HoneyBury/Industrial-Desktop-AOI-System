#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class CameraPixelFormat { Rgb24, Bgr24, Gray8 };

struct CameraFrame {
  int width {0};
  int height {0};
  int channels {0};
  CameraPixelFormat pixelFormat {CameraPixelFormat::Rgb24};
  std::vector<std::uint8_t> data;
};

class ICamera {
public:
  virtual ~ICamera() = default;

  virtual bool open(int index) = 0;
  virtual void close() = 0;
  virtual bool isOpened() const = 0;
  virtual CameraFrame grabFrame() = 0;
  virtual std::string cameraName() const = 0;
};
