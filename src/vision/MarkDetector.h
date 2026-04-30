#pragma once

#include "common/Result.h"

#include <string>
#include <vector>

struct MarkPoint {
  double x {0.0};
  double y {0.0};
  double score {0.0};
};

class MarkDetector {
public:
  Result<std::vector<MarkPoint>> detectTemplateMarks(const std::string &imagePath) const;
  Result<std::vector<MarkPoint>> detectCircularMarks(const std::string &imagePath) const;
};

