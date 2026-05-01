#pragma once

#include "common/Result.h"
#include "vision/MarkDetector.h"

#include <string>
#include <vector>

namespace alignment {

class MarkDetector {
public:
  Result<std::vector<MarkPoint>> detect(const std::string &imagePath,
                                        MarkAlgorithm preferredAlgorithm) const;
};

} // namespace alignment
