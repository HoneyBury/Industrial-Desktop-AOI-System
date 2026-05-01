#pragma once

#include "common/Result.h"

#include <string>
#include <vector>

struct AiDetection {
  std::string label;
  double confidence {0.0};
  double bboxX {0.0};
  double bboxY {0.0};
  double bboxWidth {0.0};
  double bboxHeight {0.0};
};

class AiInferencer {
public:
  Result<void> loadModel(const std::string &modelPath);
  Result<std::vector<AiDetection>> infer(const std::string &imagePath) const;
  std::string modelPath() const;

private:
  // Traditional CV fallback: basic statistical defect detection.
  Result<std::vector<AiDetection>> inferTraditional(const std::string &imagePath) const;

  std::string modelPath_;
  bool modelLoaded_ {false};

#ifdef AOI_HAS_OPENCV
  // OpenCV DNN net handle (lazy-initialized on first inference).
  mutable void *dnnNet_ {nullptr};
  mutable bool dnnAttempted_ {false};
#endif
};
