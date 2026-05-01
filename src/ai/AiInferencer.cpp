#include "ai/AiInferencer.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef AOI_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

Result<void> AiInferencer::loadModel(const std::string &modelPath) {
  if (modelPath.empty()) {
    modelLoaded_ = true; // proceed with traditional fallback
    return Result<void>::success("No model path provided; traditional CV fallback will be used.");
  }

  // Verify the model file exists.
  std::ifstream test(modelPath);
  if (!test.good()) {
    modelLoaded_ = true;
    return Result<void>::success("Model file not found at " + modelPath +
                                 "; traditional CV fallback will be used.");
  }
  test.close();

  modelPath_ = modelPath;
  modelLoaded_ = true;

#ifdef AOI_HAS_OPENCV
  // Try loading the ONNX model via OpenCV DNN.
  try {
    auto *net = new cv::dnn::Net(cv::dnn::readNetFromONNX(modelPath));
    dnnNet_ = static_cast<void *>(net);
    dnnAttempted_ = true;
    return Result<void>::success("ONNX model loaded via OpenCV DNN: " + modelPath);
  } catch (const cv::Exception &e) {
    dnnAttempted_ = true;
    // DNN load failed; traditional fallback will be used at inference time.
    return Result<void>::success("DNN failed to load ONNX model: " + std::string(e.what()) +
                                 "; traditional CV fallback will be used.");
  }
#else
  return Result<void>::success("OpenCV not available; stub inference will be used.");
#endif
}

Result<std::vector<AiDetection>> AiInferencer::infer(const std::string &imagePath) const {
  if (!modelLoaded_) {
    return Result<std::vector<AiDetection>>::failure("AI model has not been loaded.");
  }

#ifdef AOI_HAS_OPENCV
  // 1. Try OpenCV DNN inference if a net was successfully created.
  if (dnnNet_ != nullptr) {
    auto *net = static_cast<cv::dnn::Net *>(dnnNet_);
    const cv::Mat image = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (image.empty()) {
      // Fall through to traditional CV.
    } else {
      try {
        cv::Mat blob = cv::dnn::blobFromImage(image, 1.0 / 255.0, cv::Size(224, 224),
                                              cv::Scalar(0.485 * 255, 0.456 * 255, 0.406 * 255),
                                              true, false);
        net->setInput(blob);
        cv::Mat output = net->forward();

        std::vector<AiDetection> detections;

        // Output shape [1, N] or [1, num_classes] depending on model.
        if (output.dims == 2 && output.size[1] == 2) {
          // Binary classifier: [ok_score, ng_score]
          const float *data = output.ptr<float>(0);
          const float okConf = data[0];
          const float ngConf = data[1];

          AiDetection det;
          det.label = (okConf >= ngConf) ? "ok" : "ng";
          det.confidence = static_cast<double>(std::max(okConf, ngConf));
          det.bboxWidth = static_cast<double>(image.cols);
          det.bboxHeight = static_cast<double>(image.rows);
          detections.push_back(det);
        } else if (output.total() > 0) {
          // Generic: take the max-scoring class.
          cv::Point maxLoc;
          double maxVal = 0.0;
          cv::minMaxLoc(output.reshape(1, 1), nullptr, &maxVal, nullptr, &maxLoc);

          AiDetection det;
          det.label = maxVal > 0.5 ? "ok" : "ng";
          det.confidence = maxVal;
          det.bboxWidth = static_cast<double>(image.cols);
          det.bboxHeight = static_cast<double>(image.rows);
          detections.push_back(det);
        }

        if (!detections.empty()) {
          return Result<std::vector<AiDetection>>::success(detections,
                                                           "DNN inference completed for " + imagePath);
        }
      } catch (const cv::Exception &) {
        // DNN inference failed; fall through to traditional CV.
      }
    }
  }

  // 2. Traditional CV fallback.
  return inferTraditional(imagePath);
#else
  // No OpenCV: return stub result.
  return Result<std::vector<AiDetection>>::success(
      {{.label = "ok", .confidence = 0.98}},
      "Stub inference completed for " + imagePath);
#endif
}

#ifdef AOI_HAS_OPENCV
Result<std::vector<AiDetection>> AiInferencer::inferTraditional(const std::string &imagePath) const {
  const cv::Mat image = cv::imread(imagePath, cv::IMREAD_COLOR);
  if (image.empty()) {
    return Result<std::vector<AiDetection>>::failure("Failed to load image for CV analysis: " + imagePath);
  }

  cv::Mat gray;
  cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

  // Feature 1: mean intensity and standard deviation.
  cv::Scalar meanVal, stddevVal;
  cv::meanStdDev(gray, meanVal, stddevVal);
  const double intensityMean = meanVal[0];
  const double intensityStd = stddevVal[0];

  // Feature 2: edge density via Canny.
  cv::Mat edges;
  cv::Canny(gray, edges, 50, 150);
  const double edgeDensity = static_cast<double>(cv::countNonZero(edges)) /
                             (gray.rows * gray.cols);

  // Feature 3: dark region ratio via OTSU binary inverse.
  cv::Mat binary;
  cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
  const double darkRatio = static_cast<double>(cv::countNonZero(binary)) /
                           (gray.rows * gray.cols);

  // Feature 4: histogram spread (inter-quartile approximation).
  cv::Mat hist;
  const int histSize = 256;
  const float range[] = {0, 256};
  const float *histRange = {range};
  cv::calcHist(&gray, 1, nullptr, cv::Mat(), hist, 1, &histSize, &histRange);
  double cumulative = 0.0;
  double p25 = 0.0, p75 = 0.0;
  const double total = gray.rows * gray.cols;
  for (int i = 0; i < histSize; ++i) {
    cumulative += hist.at<float>(i);
    if (cumulative / total >= 0.25 && p25 == 0.0) p25 = static_cast<double>(i);
    if (cumulative / total >= 0.75 && p75 == 0.0) p75 = static_cast<double>(i);
  }
  const double histSpread = p75 - p25;

  // Heuristic scoring based on industrial inspection expectations:
  // - Too dark or too bright suggests anomaly (mean outside 30-220 range)
  // - Very high edge density suggests scratches/cracks
  // - Extreme dark ratio suggests missing parts or excessive contamination
  double anomalyScore = 0.0;

  if (intensityMean < 30.0 || intensityMean > 220.0) anomalyScore += 0.35;
  if (intensityStd > 80.0) anomalyScore += 0.20;
  if (edgeDensity > 0.25) anomalyScore += 0.25;
  if (darkRatio < 0.02 || darkRatio > 0.60) anomalyScore += 0.20;
  if (histSpread < 15.0) anomalyScore += 0.10;

  anomalyScore = std::min(anomalyScore, 1.0);
  const double okConfidence = 1.0 - anomalyScore;

  AiDetection det;
  det.label = (okConfidence >= 0.6) ? "ok" : "ng";
  det.confidence = okConfidence;
  det.bboxWidth = static_cast<double>(gray.cols);
  det.bboxHeight = static_cast<double>(gray.rows);

  std::ostringstream ss;
  ss << "Traditional CV inference: mean=" << static_cast<int>(intensityMean)
     << " std=" << static_cast<int>(intensityStd)
     << " edgeDensity=" << std::fixed << std::setprecision(3) << edgeDensity
     << " darkRatio=" << darkRatio
     << " histSpread=" << histSpread
     << "  " << det.label << " (" << det.confidence << ")";

  return Result<std::vector<AiDetection>>::success({det}, ss.str());
}
#endif

std::string AiInferencer::modelPath() const { return modelPath_; }
