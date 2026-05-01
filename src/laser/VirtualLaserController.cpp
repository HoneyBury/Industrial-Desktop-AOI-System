#include "laser/VirtualLaserController.h"

#include <sstream>

VirtualLaserController::VirtualLaserController() = default;

bool VirtualLaserController::isReady() const {
  return ready_ && !stopped_;
}

bool VirtualLaserController::executeMark(const LaserMarkingParams &params) {
  if (stopped_) {
    lastReport_ = "Laser is emergency-stopped; cannot fire.";
    return false;
  }

  std::ostringstream ss;
  ss << "Virtual laser fired at (" << params.x << ", " << params.y << ")"
     << " power=" << params.powerPercent << "%"
     << " freq=" << params.frequencyKhz << "kHz"
     << " pulse=" << params.pulseWidthUs << "us"
     << " repeat=" << params.repeatCount;
  lastReport_ = ss.str();
  return true;
}

void VirtualLaserController::emergencyStop() {
  stopped_ = true;
  ready_ = false;
}

void VirtualLaserController::resetEmergencyStop() {
  stopped_ = false;
  ready_ = true;
}

bool VirtualLaserController::isStopped() const { return stopped_; }

std::string VirtualLaserController::lastMarkReport() const { return lastReport_; }
