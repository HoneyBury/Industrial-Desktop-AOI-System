#include "ai/AiInferencer.h"

Result<void> AiInferencer::loadModel(const std::string &modelPath) {
  if (modelPath.empty()) {
    return Result<void>::failure("Model path is empty.");
  }

  modelPath_ = modelPath;
  return Result<void>::success("AI model loaded in bootstrap mode.");
}

Result<std::vector<AiDetection>> AiInferencer::infer(const std::string &imagePath) const {
  if (modelPath_.empty()) {
    return Result<std::vector<AiDetection>>::failure("AI model has not been loaded.");
  }

  return Result<std::vector<AiDetection>>::success(
      {{.label = "ok", .confidence = 0.98}},
      "Stub ONNX inference finished for " + imagePath);
}

std::string AiInferencer::modelPath() const { return modelPath_; }

