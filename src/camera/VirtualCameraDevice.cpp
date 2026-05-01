#include "camera/VirtualCameraDevice.h"

#include <algorithm>
#include <cmath>

#ifdef AOI_HAS_OPENCV
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace {

MechanicalPose defaultBoardReadyOrigin(const BoardDefinition &definition) {
  return MechanicalPose {-definition.boardLengthMm, -definition.boardWidthMm, 0.0, 0.0};
}

#ifdef AOI_HAS_OPENCV
CameraFrame matToCameraFrame(const cv::Mat &frame) {
  CameraFrame result;
  if (frame.empty()) {
    return result;
  }

  result.width = frame.cols;
  result.height = frame.rows;
  result.channels = frame.channels();
  result.pixelFormat = frame.channels() == 1 ? CameraPixelFormat::Gray8 : CameraPixelFormat::Bgr24;
  result.data.assign(frame.datastart, frame.dataend);
  return result;
}
#endif

#ifdef AOI_HAS_OPENCV
void drawCenteredCrosshair(cv::Mat &image) {
  const cv::Point center(image.cols / 2, image.rows / 2);
  const cv::Scalar crosshairColor(54, 211, 153);
  cv::line(image, cv::Point(center.x - 18, center.y), cv::Point(center.x + 18, center.y), crosshairColor, 1);
  cv::line(image, cv::Point(center.x, center.y - 18), cv::Point(center.x, center.y + 18), crosshairColor, 1);
  cv::circle(image, center, 22, crosshairColor, 1);
}

void drawOverlayText(cv::Mat &image, const VirtualCameraPoseInfo &poseInfo, const ScanRecipe &recipe) {
  cv::rectangle(image, cv::Rect(18, 18, std::min(430, image.cols - 36), 126), cv::Scalar(7, 11, 24), cv::FILLED);
  cv::rectangle(image, cv::Rect(18, 18, std::min(430, image.cols - 36), 126), cv::Scalar(56, 189, 248), 1);

  const std::string stateText = poseInfo.transportState == BoardTransportState::Loading
                                    ? "Loading"
                                    : poseInfo.transportState == BoardTransportState::BoardReady
                                          ? "BoardReady"
                                          : poseInfo.transportState == BoardTransportState::Unloading ? "Unloading"
                                                                                                      : "Idle";
  const std::string boardText = cv::format("Board XY: %.2f, %.2f mm",
                                           poseInfo.logicalBoardCenterXmm,
                                           poseInfo.logicalBoardCenterYmm);
  const std::string cardText = cv::format("Card XY: %.2f, %.2f mm",
                                          poseInfo.physicalCardXmm,
                                          poseInfo.physicalCardYmm);
  const std::string fovText = cv::format("FOV: %.1f x %.1f mm", recipe.fovWidthMm, recipe.fovHeightMm);

  int baseline = 0;
  cv::putText(image, "Virtual Board Camera", cv::Point(32, 48), cv::FONT_HERSHEY_SIMPLEX, 0.7,
              cv::Scalar(248, 250, 252), 2, cv::LINE_AA);
  cv::putText(image, ("State: " + stateText), cv::Point(32, 76), cv::FONT_HERSHEY_SIMPLEX, 0.56,
              cv::Scalar(148, 163, 184), 1, cv::LINE_AA);
  cv::putText(image, boardText, cv::Point(32, 102), cv::FONT_HERSHEY_SIMPLEX, 0.56,
              cv::Scalar(226, 232, 240), 1, cv::LINE_AA);
  cv::putText(image, cardText, cv::Point(32, 128), cv::FONT_HERSHEY_SIMPLEX, 0.56,
              cv::Scalar(226, 232, 240), 1, cv::LINE_AA);
  cv::getTextSize(fovText, cv::FONT_HERSHEY_SIMPLEX, 0.56, 1, &baseline);
  cv::putText(image, fovText, cv::Point(image.cols - 220, image.rows - 24), cv::FONT_HERSHEY_SIMPLEX, 0.56,
              cv::Scalar(250, 204, 21), 1, cv::LINE_AA);
}

cv::Mat renderEmptyFrame(const int width, const int height, const VirtualCameraPoseInfo &poseInfo, const ScanRecipe &recipe) {
  cv::Mat frame(height, width, CV_8UC3, cv::Scalar(10, 18, 32));

  for (int x = 0; x < frame.cols; x += 48) {
    cv::line(frame, cv::Point(x, 0), cv::Point(x, frame.rows), cv::Scalar(22, 34, 54), 1);
  }
  for (int y = 0; y < frame.rows; y += 48) {
    cv::line(frame, cv::Point(0, y), cv::Point(frame.cols, y), cv::Scalar(22, 34, 54), 1);
  }

  drawCenteredCrosshair(frame);
  cv::putText(frame, "No board under current FOV", cv::Point(40, frame.rows / 2), cv::FONT_HERSHEY_SIMPLEX, 0.9,
              cv::Scalar(226, 232, 240), 2, cv::LINE_AA);
  drawOverlayText(frame, poseInfo, recipe);
  return frame;
}
#endif

} // namespace

void VirtualCameraDevice::setPreferredFrameSize(const int width, const int height) {
  if (width > 0) {
    preferredFrameWidth_ = width;
  }
  if (height > 0) {
    preferredFrameHeight_ = height;
  }
}

void VirtualCameraDevice::setBoardImagePath(std::string path) {
  boardImagePath_ = std::move(path);
#ifdef AOI_HAS_OPENCV
  boardImageLoaded_ = false;
  boardImage_.release();
#endif
}

void VirtualCameraDevice::setBoardDefinition(const BoardDefinition &definition) {
  boardDefinition_ = definition;
}

void VirtualCameraDevice::setScanRecipe(const ScanRecipe &recipe) {
  scanRecipe_ = recipe;
}

void VirtualCameraDevice::setCurrentPose(const MechanicalPose &pose) {
  currentPose_ = pose;
}

void VirtualCameraDevice::setBoardReadyOriginPose(const MechanicalPose &pose) {
  boardReadyOriginPose_ = pose;
  hasExplicitReadyOrigin_ = true;
}

void VirtualCameraDevice::clearBoardReadyOriginPose() {
  boardReadyOriginPose_ = {};
  hasExplicitReadyOrigin_ = false;
}

void VirtualCameraDevice::setTransportState(const BoardTransportState state,
                                            const double boardPositionMm,
                                            const double stopperTargetMm) {
  transportState_ = state;
  boardPositionMm_ = boardPositionMm;
  stopperTargetMm_ = stopperTargetMm;
}

bool VirtualCameraDevice::open(const int /*index*/) {
#ifdef AOI_HAS_OPENCV
  if (!ensureBoardImageLoaded()) {
    open_ = false;
    return false;
  }
#endif
  open_ = true;
  return true;
}

void VirtualCameraDevice::close() { open_ = false; }

bool VirtualCameraDevice::isOpened() const { return open_; }

CameraFrame VirtualCameraDevice::grabFrame() {
  if (!open_) {
    return {};
  }

  lastPoseInfo_ = computePoseInfo();

#ifdef AOI_HAS_OPENCV
  return renderOpenCvFrame();
#else
  return buildStubFrame();
#endif
}

std::string VirtualCameraDevice::cameraName() const { return "Virtual Demo Board Camera"; }

const VirtualCameraPoseInfo &VirtualCameraDevice::lastPoseInfo() const { return lastPoseInfo_; }

MechanicalPose VirtualCameraDevice::effectiveReadyOriginPose() const {
  return hasExplicitReadyOrigin_ ? boardReadyOriginPose_ : defaultBoardReadyOrigin(boardDefinition_);
}

VirtualCameraPoseInfo VirtualCameraDevice::computePoseInfo() const {
  VirtualCameraPoseInfo poseInfo;
  poseInfo.transportState = transportState_;
  poseInfo.boardVisible = transportState_ != BoardTransportState::Idle;
  poseInfo.boardTransportOffsetXmm = stopperTargetMm_ - boardPositionMm_;

  const MechanicalPose readyOrigin = effectiveReadyOriginPose();
  poseInfo.boardOriginXmm = readyOrigin.x + poseInfo.boardTransportOffsetXmm;
  poseInfo.boardOriginYmm = readyOrigin.y;
  poseInfo.logicalBoardCenterXmm = currentPose_.x - poseInfo.boardOriginXmm;
  poseInfo.logicalBoardCenterYmm = currentPose_.y - poseInfo.boardOriginYmm;
  poseInfo.physicalCardXmm = boardDefinition_.boardLengthMm - poseInfo.logicalBoardCenterXmm;
  poseInfo.physicalCardYmm = boardDefinition_.boardWidthMm - poseInfo.logicalBoardCenterYmm;
  poseInfo.centerInsideBoard =
      poseInfo.logicalBoardCenterXmm >= 0.0 &&
      poseInfo.logicalBoardCenterXmm <= boardDefinition_.boardLengthMm &&
      poseInfo.logicalBoardCenterYmm >= 0.0 &&
      poseInfo.logicalBoardCenterYmm <= boardDefinition_.boardWidthMm;
  return poseInfo;
}

CameraFrame VirtualCameraDevice::buildStubFrame() const {
  const int width = std::max(preferredFrameWidth_, 320);
  const int height = std::max(preferredFrameHeight_, 240);
  CameraFrame frame;
  frame.width = width;
  frame.height = height;
  frame.channels = 3;
  frame.pixelFormat = CameraPixelFormat::Rgb24;
  frame.data.resize(static_cast<std::size_t>(width * height * 3));

  const double xSeed = std::clamp(lastPoseInfo_.logicalBoardCenterXmm, -boardDefinition_.boardLengthMm,
                                  boardDefinition_.boardLengthMm * 2.0);
  const double ySeed = std::clamp(lastPoseInfo_.logicalBoardCenterYmm, -boardDefinition_.boardWidthMm,
                                  boardDefinition_.boardWidthMm * 2.0);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t offset = static_cast<std::size_t>((y * width + x) * 3);
      frame.data[offset] = static_cast<std::uint8_t>(std::fmod(x + xSeed * 7.0, 255.0));
      frame.data[offset + 1] = static_cast<std::uint8_t>(std::fmod(y + ySeed * 9.0, 255.0));
      frame.data[offset + 2] = lastPoseInfo_.boardVisible ? 180U : 64U;
    }
  }

  return frame;
}

#ifdef AOI_HAS_OPENCV
bool VirtualCameraDevice::ensureBoardImageLoaded() {
  if (boardImageLoaded_ && !boardImage_.empty()) {
    return true;
  }
  if (boardImagePath_.empty()) {
    return false;
  }

  boardImage_ = cv::imread(boardImagePath_, cv::IMREAD_COLOR);
  boardImageLoaded_ = !boardImage_.empty();
  return boardImageLoaded_;
}

CameraFrame VirtualCameraDevice::renderOpenCvFrame() {
  if (!ensureBoardImageLoaded()) {
    return {};
  }

  const int width = std::max(preferredFrameWidth_, 320);
  const int height = std::max(preferredFrameHeight_, 240);
  const double boardLength = std::max(boardDefinition_.boardLengthMm, 1.0);
  const double boardWidth = std::max(boardDefinition_.boardWidthMm, 1.0);
  const double fovWidth = std::clamp(scanRecipe_.fovWidthMm > 0.0 ? scanRecipe_.fovWidthMm : 32.0, 1.0, boardLength);
  const double fovHeight = std::clamp(scanRecipe_.fovHeightMm > 0.0 ? scanRecipe_.fovHeightMm : 24.0, 1.0, boardWidth);

  if (!lastPoseInfo_.boardVisible) {
    return matToCameraFrame(renderEmptyFrame(width, height, lastPoseInfo_, scanRecipe_));
  }

  const int cropWidthPx = std::max(1, static_cast<int>(std::lround(boardImage_.cols * fovWidth / boardLength)));
  const int cropHeightPx = std::max(1, static_cast<int>(std::lround(boardImage_.rows * fovHeight / boardWidth)));

  const double centerXPx =
      (std::clamp(lastPoseInfo_.logicalBoardCenterXmm, -fovWidth, boardLength + fovWidth) / boardLength) * boardImage_.cols;
  const double centerYPx =
      (std::clamp(lastPoseInfo_.logicalBoardCenterYmm, -fovHeight, boardWidth + fovHeight) / boardWidth) * boardImage_.rows;
  const int cropLeftPx = static_cast<int>(std::lround(centerXPx - cropWidthPx / 2.0));
  const int cropTopPx = static_cast<int>(std::lround(centerYPx - cropHeightPx / 2.0));

  cv::Mat cropCanvas(cropHeightPx, cropWidthPx, CV_8UC3, cv::Scalar(10, 18, 32));
  const cv::Rect requested(cropLeftPx, cropTopPx, cropWidthPx, cropHeightPx);
  const cv::Rect sourceBounds(0, 0, boardImage_.cols, boardImage_.rows);
  const cv::Rect clipped = requested & sourceBounds;
  if (clipped.width > 0 && clipped.height > 0) {
    const cv::Rect target(clipped.x - requested.x, clipped.y - requested.y, clipped.width, clipped.height);
    boardImage_(clipped).copyTo(cropCanvas(target));
  }

  cv::Mat frame;
  cv::resize(cropCanvas, frame, cv::Size(width, height), 0.0, 0.0, cv::INTER_LINEAR);

  if (!lastPoseInfo_.centerInsideBoard) {
    cv::rectangle(frame, cv::Rect(0, 0, frame.cols, frame.rows), cv::Scalar(30, 64, 175), 10);
  } else {
    cv::rectangle(frame, cv::Rect(10, 10, frame.cols - 20, frame.rows - 20), cv::Scalar(56, 189, 248), 2);
  }

  drawCenteredCrosshair(frame);
  drawOverlayText(frame, lastPoseInfo_, scanRecipe_);
  return matToCameraFrame(frame);
}
#endif
