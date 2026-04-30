#pragma once

#include <utility>

struct PixelPoint {
  double x {0.0};
  double y {0.0};
};

struct MillimeterPoint {
  double x {0.0};
  double y {0.0};
};

struct MechanicalPose {
  double x {0.0};
  double y {0.0};
  double z {0.0};
  double r {0.0};
};

class CoordinateTransformer {
public:
  CoordinateTransformer(double pixelToMillimeterX = 0.01, double pixelToMillimeterY = 0.01);

  MillimeterPoint pixelToMillimeter(const PixelPoint &pixelOffset) const;
  PixelPoint millimeterToPixel(const MillimeterPoint &millimeterOffset) const;
  MechanicalPose productToMechanical(const MillimeterPoint &productPoint,
                                     const MechanicalPose &originPose) const;
  double computeMarkRotationDegrees(const std::pair<PixelPoint, PixelPoint> &referenceMarkPair,
                                    const std::pair<PixelPoint, PixelPoint> &measuredMarkPair) const;

private:
  double pixelToMillimeterX_ {0.01};
  double pixelToMillimeterY_ {0.01};
};

