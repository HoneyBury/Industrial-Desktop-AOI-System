#pragma once

#include <string>

struct LaserMarkingParams {
  double x {0.0};
  double y {0.0};
  double powerPercent {80.0};
  double frequencyKhz {20.0};
  double pulseWidthUs {10.0};
  int repeatCount {1};
};

class ILaserController {
public:
  virtual ~ILaserController() = default;

  virtual bool isReady() const = 0;
  virtual bool executeMark(const LaserMarkingParams &params) = 0;
  virtual void emergencyStop() = 0;
  virtual void resetEmergencyStop() = 0;
  virtual bool isStopped() const = 0;
  virtual std::string lastMarkReport() const = 0;
};
