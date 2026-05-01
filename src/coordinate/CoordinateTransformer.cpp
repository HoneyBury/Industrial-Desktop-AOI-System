#include "coordinate/CoordinateTransformer.h"

#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

double toRadians(const double degrees) { return degrees * kPi / 180.0; }

double angleDegrees(const PixelPoint &from, const PixelPoint &to) {
  return std::atan2(to.y - from.y, to.x - from.x) * 180.0 / kPi;
}

MillimeterPoint rotatePoint(const MillimeterPoint &point, const double rotationDegrees) {
  const double radians = toRadians(rotationDegrees);
  const double cosValue = std::cos(radians);
  const double sinValue = std::sin(radians);
  return MillimeterPoint {point.x * cosValue - point.y * sinValue,
                          point.x * sinValue + point.y * cosValue};
}

} // namespace

CoordinateTransformer::CoordinateTransformer(const double pixelToMillimeterX,
                                             const double pixelToMillimeterY) {
  chain_.pixelToMillimeterX = pixelToMillimeterX;
  chain_.pixelToMillimeterY = pixelToMillimeterY;
}

CoordinateTransformer::CoordinateTransformer(CoordinateTransformChain chain) : chain_(std::move(chain)) {}

const CoordinateTransformChain &CoordinateTransformer::chain() const { return chain_; }

void CoordinateTransformer::setChain(CoordinateTransformChain chain) { chain_ = std::move(chain); }

UndistortedPixelPoint CoordinateTransformer::cameraPixelToUndistorted(const PixelPoint &cameraPixel) const {
  const double dx = cameraPixel.x - chain_.opticalCenterPixel.x;
  const double dy = cameraPixel.y - chain_.opticalCenterPixel.y;

  if (chain_.distortionCoefficients.size() < 2) {
    return UndistortedPixelPoint {cameraPixel.x, cameraPixel.y};
  }

  const double k1 = chain_.distortionCoefficients[0];
  const double k2 = chain_.distortionCoefficients[1];
  const double normalizedRadiusSquared = dx * dx + dy * dy;
  const double scale = 1.0 + k1 * normalizedRadiusSquared + k2 * normalizedRadiusSquared * normalizedRadiusSquared;

  return UndistortedPixelPoint {chain_.opticalCenterPixel.x + dx * scale,
                                chain_.opticalCenterPixel.y + dy * scale};
}

MillimeterPoint CoordinateTransformer::pixelToMillimeter(const PixelPoint &pixelOffset) const {
  return MillimeterPoint {pixelOffset.x * chain_.pixelToMillimeterX,
                          pixelOffset.y * chain_.pixelToMillimeterY};
}

PixelPoint CoordinateTransformer::millimeterToPixel(const MillimeterPoint &millimeterOffset) const {
  return PixelPoint {millimeterOffset.x / chain_.pixelToMillimeterX,
                     millimeterOffset.y / chain_.pixelToMillimeterY};
}

MillimeterPoint CoordinateTransformer::undistortedPixelToImagePhysical(
    const UndistortedPixelPoint &pixelPoint) const {
  const PixelPoint relativePixel {pixelPoint.x - chain_.opticalCenterPixel.x,
                                  pixelPoint.y - chain_.opticalCenterPixel.y};
  return pixelToMillimeter(relativePixel);
}

MillimeterPoint CoordinateTransformer::cameraPixelToImagePhysical(const PixelPoint &cameraPixel) const {
  return undistortedPixelToImagePhysical(cameraPixelToUndistorted(cameraPixel));
}

MillimeterPoint CoordinateTransformer::imagePhysicalToProduct(
    const MillimeterPoint &imagePhysicalPoint) const {
  return applyRigidTransform(imagePhysicalPoint, chain_.imagePhysicalToProduct);
}

MechanicalPose CoordinateTransformer::productToMechanical(const MillimeterPoint &productPoint,
                                                          const MechanicalPose &originPose) const {
  const MillimeterPoint machinePoint = applyRigidTransform(productPoint, chain_.productToMachine);
  MechanicalPose pose = originPose;
  pose.x += machinePoint.x;
  pose.y += machinePoint.y;
  pose.r += chain_.productToMachine.rotationDegrees;
  return pose;
}

MechanicalPose CoordinateTransformer::imagePhysicalToMechanical(
    const MillimeterPoint &imagePhysicalPoint, const MechanicalPose &originPose) const {
  return productToMechanical(imagePhysicalToProduct(imagePhysicalPoint), originPose);
}

MechanicalPose CoordinateTransformer::machineToLaser(const MechanicalPose &machinePose) const {
  MechanicalPose laserPose = machinePose;
  laserPose.x += chain_.machineToLaserOffset.x;
  laserPose.y += chain_.machineToLaserOffset.y;
  return laserPose;
}

MechanicalPose CoordinateTransformer::cameraPixelToLaser(const PixelPoint &cameraPixel,
                                                         const MechanicalPose &originPose) const {
  return machineToLaser(imagePhysicalToMechanical(cameraPixelToImagePhysical(cameraPixel), originPose));
}

double CoordinateTransformer::computeMarkRotationDegrees(
    const std::pair<PixelPoint, PixelPoint> &referenceMarkPair,
    const std::pair<PixelPoint, PixelPoint> &measuredMarkPair) const {
  const double referenceAngle = angleDegrees(referenceMarkPair.first, referenceMarkPair.second);
  const double measuredAngle = angleDegrees(measuredMarkPair.first, measuredMarkPair.second);
  return measuredAngle - referenceAngle;
}

MillimeterPoint CoordinateTransformer::machineToProduct(
    const MechanicalPose &machinePose, const MechanicalPose &originPose) const {
  // Inverse of productToMechanical:
  //   productToMechanical: product → machinePose
  //     relativeMachine = rotate(product, rotDeg) + {tx, ty}
  //     pose.x += relativeMachine.x; pose.y += relativeMachine.y;
  //   machineToProduct: machinePose → product
  //     relativeMachine = {machinePose.x - originPose.x, machinePose.y - originPose.y}
  //     product = rotate(relativeMachine - {tx, ty}, -rotDeg)
  const double relativeX = machinePose.x - originPose.x;
  const double relativeY = machinePose.y - originPose.y;
  const double tx = chain_.productToMachine.translationX;
  const double ty = chain_.productToMachine.translationY;
  return rotatePoint({relativeX - tx, relativeY - ty}, -chain_.productToMachine.rotationDegrees);
}

MillimeterPoint CoordinateTransformer::applyMarkTransform(
    const MillimeterPoint &productPoint, const RigidTransform2D &markTransform) {
  return applyRigidTransform(productPoint, markTransform);
}

MechanicalPose CoordinateTransformer::applyLaserOffset(
    const MechanicalPose &machinePose, const MillimeterPoint &laserOffsetMm) const {
  MechanicalPose result = machinePose;
  result.x += laserOffsetMm.x;
  result.y += laserOffsetMm.y;
  return result;
}

MillimeterPoint CoordinateTransformer::imageClickToMoveDelta(
    const PixelPoint &clickPixel, const PixelPoint &imageCenter) const {
  const PixelPoint pixelOffset{clickPixel.x - imageCenter.x, clickPixel.y - imageCenter.y};
  return pixelToMillimeter(pixelOffset);
}

MechanicalPose CoordinateTransformer::targetProductPointToCameraPosition(
    const MillimeterPoint &productPoint, const MechanicalPose &originPose) const {
  return productToMechanical(productPoint, originPose);
}

MechanicalPose CoordinateTransformer::targetProductPointToLaserPosition(
    const MillimeterPoint &productPoint, const MechanicalPose &originPose,
    const MillimeterPoint &laserOffsetMm) const {
  const MechanicalPose cameraPose = productToMechanical(productPoint, originPose);
  return applyLaserOffset(cameraPose, laserOffsetMm);
}

MillimeterPoint CoordinateTransformer::applyRigidTransform(const MillimeterPoint &point,
                                                           const RigidTransform2D &transform) {
  const MillimeterPoint rotatedPoint = rotatePoint(point, transform.rotationDegrees);
  return MillimeterPoint {rotatedPoint.x + transform.translationX,
                          rotatedPoint.y + transform.translationY};
}
