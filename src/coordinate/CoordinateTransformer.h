#pragma once

#include <array>
#include <utility>
#include <vector>

enum class CoordinateFrameType {
  CameraPixel,
  UndistortedPixel,
  ImagePhysicalMm,
  Product,
  Machine,
  Laser,
};

struct PixelPoint {
  double x {0.0};
  double y {0.0};
};

struct UndistortedPixelPoint {
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

struct RigidTransform2D {
  double translationX {0.0};
  double translationY {0.0};
  double rotationDegrees {0.0};
};

struct CoordinateTransformChain {
  PixelPoint opticalCenterPixel;
  std::vector<double> distortionCoefficients;
  double pixelToMillimeterX {0.01};
  double pixelToMillimeterY {0.01};
  RigidTransform2D imagePhysicalToProduct;
  RigidTransform2D productToMachine;
  MillimeterPoint machineToLaserOffset;
};

class CoordinateTransformer {
public:
  CoordinateTransformer(double pixelToMillimeterX = 0.01, double pixelToMillimeterY = 0.01);
  explicit CoordinateTransformer(CoordinateTransformChain chain);

  [[nodiscard]] const CoordinateTransformChain &chain() const;
  void setChain(CoordinateTransformChain chain);

  [[nodiscard]] UndistortedPixelPoint cameraPixelToUndistorted(const PixelPoint &cameraPixel) const;
  [[nodiscard]] MillimeterPoint pixelToMillimeter(const PixelPoint &pixelOffset) const;
  [[nodiscard]] PixelPoint millimeterToPixel(const MillimeterPoint &millimeterOffset) const;
  [[nodiscard]] MillimeterPoint undistortedPixelToImagePhysical(
      const UndistortedPixelPoint &pixelPoint) const;
  [[nodiscard]] MillimeterPoint cameraPixelToImagePhysical(const PixelPoint &cameraPixel) const;
  [[nodiscard]] MillimeterPoint imagePhysicalToProduct(const MillimeterPoint &imagePhysicalPoint) const;
  [[nodiscard]] MechanicalPose productToMechanical(const MillimeterPoint &productPoint,
                                                   const MechanicalPose &originPose) const;
  [[nodiscard]] MechanicalPose imagePhysicalToMechanical(const MillimeterPoint &imagePhysicalPoint,
                                                         const MechanicalPose &originPose) const;
  [[nodiscard]] MechanicalPose machineToLaser(const MechanicalPose &machinePose) const;
  [[nodiscard]] MechanicalPose cameraPixelToLaser(const PixelPoint &cameraPixel,
                                                  const MechanicalPose &originPose) const;

  [[nodiscard]] double computeMarkRotationDegrees(
      const std::pair<PixelPoint, PixelPoint> &referenceMarkPair,
      const std::pair<PixelPoint, PixelPoint> &measuredMarkPair) const;

  [[nodiscard]] static MillimeterPoint applyRigidTransform(const MillimeterPoint &point,
                                                           const RigidTransform2D &transform);

private:
  CoordinateTransformChain chain_ {};
};
