#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct CameraFrame {
  int width {0};
  int height {0};
  int channels {0};
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

