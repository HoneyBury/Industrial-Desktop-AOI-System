#pragma once

#include "laser/ILaserController.h"

#include <string>

class VirtualLaserController final : public ILaserController {
public:
  VirtualLaserController();

  bool isReady() const override;
  bool executeMark(const LaserMarkingParams &params) override;
  void emergencyStop() override;
  void resetEmergencyStop() override;
  bool isStopped() const override;
  std::string lastMarkReport() const override;

private:
  bool ready_ {true};
  bool stopped_ {false};
  std::string lastReport_;
};
