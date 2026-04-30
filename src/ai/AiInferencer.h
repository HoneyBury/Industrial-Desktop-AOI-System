#pragma once

#include "common/Result.h"

#include <string>
#include <vector>

struct AiDetection {
  std::string label;
  double confidence {0.0};
};

class AiInferencer {
public:
  Result<void> loadModel(const std::string &modelPath);
  Result<std::vector<AiDetection>> infer(const std::string &imagePath) const;
  std::string modelPath() const;

private:
  std::string modelPath_;
};

