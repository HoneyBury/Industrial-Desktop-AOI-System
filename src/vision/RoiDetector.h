#pragma once

#include "common/Result.h"

#include <string>
#include <vector>

struct RoiRegion {
  double x {0.0};
  double y {0.0};
  double width {0.0};
  double height {0.0};
};

class RoiDetector {
public:
  Result<std::vector<RoiRegion>> detectByThreshold(const std::string &imagePath) const;
  Result<std::vector<RoiRegion>> detectByTemplate(const std::string &imagePath) const;
};

