#pragma once

#include "common/Result.h"

#include <string>
#include <string_view>
#include <vector>

enum class RoiShape {
  Rectangle,
  Circle,
};

inline constexpr std::string_view toString(const RoiShape shape) {
  switch (shape) {
  case RoiShape::Rectangle:
    return "rectangle";
  case RoiShape::Circle:
    return "circle";
  }

  return "rectangle";
}

inline RoiShape roiShapeFromString(const std::string &value) {
  if (value == "circle") {
    return RoiShape::Circle;
  }

  return RoiShape::Rectangle;
}

struct RoiRegion {
  std::string name {"ROI"};
  double x {0.0};
  double y {0.0};
  double width {0.0};
  double height {0.0};
  double rotation {0.0};
  double threshold {0.78};
  RoiShape shape {RoiShape::Rectangle};
  bool enabled {true};
};

class RoiDetector {
public:
  Result<std::vector<RoiRegion>> detectByThreshold(const std::string &imagePath) const;
  Result<std::vector<RoiRegion>> detectByTemplate(const std::string &imagePath) const;
};
