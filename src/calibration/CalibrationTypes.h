#pragma once

#include "coordinate/CoordinateTransformer.h"

#include <string>
#include <vector>

struct CameraIntrinsicCalibration {
  bool calibrated {false};
  double fx {0.0};
  double fy {0.0};
  double cx {0.0};
  double cy {0.0};
  std::vector<double> distortionCoefficients;
};

struct PixelScaleCalibration {
  bool calibrated {false};
  double pixelToMillimeterX {0.01};
  double pixelToMillimeterY {0.01};
};

struct OriginCalibration {
  bool calibrated {false};
  PixelPoint imageReferencePixel;
  MechanicalPose machineReferencePose;
};

struct LaserOffsetCalibration {
  bool calibrated {false};
  double cameraToLaserDxMm {0.0};
  double cameraToLaserDyMm {0.0};
};

struct MarkReferenceRecord {
  std::string name;
  PixelPoint referencePixel;
  MillimeterPoint referenceProductPoint;
  bool enabled {true};
};
