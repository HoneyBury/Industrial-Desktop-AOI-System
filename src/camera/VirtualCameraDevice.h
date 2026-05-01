#pragma once

#include "camera/ICamera.h"
#include "coordinate/CoordinateTransformer.h"
#include "program/ProgramModel.h"
#include "transport/ITransportController.h"

#include <string>

#ifdef AOI_HAS_OPENCV
#include <opencv2/core/mat.hpp>
#endif

struct VirtualCameraPoseInfo {
  bool boardVisible {false};
  bool centerInsideBoard {false};
  BoardTransportState transportState {BoardTransportState::Idle};
  double logicalBoardCenterXmm {0.0};
  double logicalBoardCenterYmm {0.0};
  double physicalCardXmm {0.0};
  double physicalCardYmm {0.0};
  double boardOriginXmm {0.0};
  double boardOriginYmm {0.0};
  double boardTransportOffsetXmm {0.0};
};

class VirtualCameraDevice final : public ICamera {
public:
  void setPreferredFrameSize(int width, int height);
  void setBoardImagePath(std::string path);
  void setBoardDefinition(const BoardDefinition &definition);
  void setScanRecipe(const ScanRecipe &recipe);
  void setCurrentPose(const MechanicalPose &pose);
  void setBoardReadyOriginPose(const MechanicalPose &pose);
  void clearBoardReadyOriginPose();
  void setTransportState(BoardTransportState state, double boardPositionMm, double stopperTargetMm);

  bool open(int index) override;
  void close() override;
  bool isOpened() const override;
  CameraFrame grabFrame() override;
  std::string cameraName() const override;

  [[nodiscard]] const VirtualCameraPoseInfo &lastPoseInfo() const;

private:
  [[nodiscard]] MechanicalPose effectiveReadyOriginPose() const;
  [[nodiscard]] VirtualCameraPoseInfo computePoseInfo() const;
  [[nodiscard]] CameraFrame buildStubFrame() const;

  std::string boardImagePath_;
  BoardDefinition boardDefinition_ {};
  ScanRecipe scanRecipe_ {};
  MechanicalPose currentPose_ {};
  MechanicalPose boardReadyOriginPose_ {};
  BoardTransportState transportState_ {BoardTransportState::Idle};
  double boardPositionMm_ {0.0};
  double stopperTargetMm_ {450.0};
  int preferredFrameWidth_ {1280};
  int preferredFrameHeight_ {720};
  bool open_ {false};
  bool hasExplicitReadyOrigin_ {false};
  VirtualCameraPoseInfo lastPoseInfo_ {};

#ifdef AOI_HAS_OPENCV
  bool ensureBoardImageLoaded();
  [[nodiscard]] CameraFrame renderOpenCvFrame();

  bool boardImageLoaded_ {false};
  cv::Mat boardImage_;
#endif
};
