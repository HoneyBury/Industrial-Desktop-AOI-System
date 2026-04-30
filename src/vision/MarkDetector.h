#pragma once

#include "common/Result.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

enum class MarkShape {
  Rectangle,
  Circle,
  Diamond,
  Cross,
};

enum class MarkAlgorithm {
  ColorBrushTemplate,
  BinaryGeometry,
};

inline constexpr std::string_view toString(const MarkShape shape) {
  switch (shape) {
  case MarkShape::Rectangle:
    return "rectangle";
  case MarkShape::Circle:
    return "circle";
  case MarkShape::Diamond:
    return "diamond";
  case MarkShape::Cross:
    return "cross";
  }

  return "rectangle";
}

inline constexpr std::string_view toString(const MarkAlgorithm algorithm) {
  switch (algorithm) {
  case MarkAlgorithm::ColorBrushTemplate:
    return "color_brush_template";
  case MarkAlgorithm::BinaryGeometry:
    return "binary_geometry";
  }

  return "color_brush_template";
}

inline MarkShape markShapeFromString(const std::string &value) {
  if (value == "circle") {
    return MarkShape::Circle;
  }

  if (value == "diamond") {
    return MarkShape::Diamond;
  }

  if (value == "cross") {
    return MarkShape::Cross;
  }

  return MarkShape::Rectangle;
}

inline MarkAlgorithm markAlgorithmFromString(const std::string &value) {
  if (value == "binary_geometry") {
    return MarkAlgorithm::BinaryGeometry;
  }

  return MarkAlgorithm::ColorBrushTemplate;
}

struct MarkPoint {
  std::string name {"Mark"};
  double x {0.0};
  double y {0.0};
  double width {48.0};
  double height {48.0};
  double rotation {0.0};
  double score {0.0};
  double minimumScore {0.8};
  double previewScore {0.86};
  int colorTolerance {18};
  std::string sampledColor {"#ff4d4f"};
  MarkShape shape {MarkShape::Rectangle};
  MarkAlgorithm algorithm {MarkAlgorithm::ColorBrushTemplate};
  bool enabled {true};
};

class MarkDetector {
public:
  Result<std::vector<MarkPoint>> detectTemplateMarks(const std::string &imagePath) const;
  Result<std::vector<MarkPoint>> detectCircularMarks(const std::string &imagePath) const;
};
