#include "vision/CoordinateTransformer.h"

#include <cmath>

namespace {

// 通过两点连线角度计算 Mark 姿态，后续可替换为更高精度的亚像素结果。
double angleDegrees(const PixelPoint &from, const PixelPoint &to) {
  return std::atan2(to.y - from.y, to.x - from.x) * 180.0 / 3.14159265358979323846;
}

} // namespace

CoordinateTransformer::CoordinateTransformer(const double pixelToMillimeterX,
                                             const double pixelToMillimeterY)
    : pixelToMillimeterX_(pixelToMillimeterX), pixelToMillimeterY_(pixelToMillimeterY) {}

MillimeterPoint CoordinateTransformer::pixelToMillimeter(const PixelPoint &pixelOffset) const {
  return MillimeterPoint {pixelOffset.x * pixelToMillimeterX_, pixelOffset.y * pixelToMillimeterY_};
}

PixelPoint CoordinateTransformer::millimeterToPixel(const MillimeterPoint &millimeterOffset) const {
  return PixelPoint {millimeterOffset.x / pixelToMillimeterX_,
                     millimeterOffset.y / pixelToMillimeterY_};
}

MechanicalPose CoordinateTransformer::productToMechanical(const MillimeterPoint &productPoint,
                                                          const MechanicalPose &originPose) const {
  MechanicalPose pose = originPose;
  pose.x += productPoint.x;
  pose.y += productPoint.y;
  return pose;
}

double CoordinateTransformer::computeMarkRotationDegrees(
    const std::pair<PixelPoint, PixelPoint> &referenceMarkPair,
    const std::pair<PixelPoint, PixelPoint> &measuredMarkPair) const {
  const double referenceAngle = angleDegrees(referenceMarkPair.first, referenceMarkPair.second);
  const double measuredAngle = angleDegrees(measuredMarkPair.first, measuredMarkPair.second);
  // 检测到的姿态角减去标准程序角度，即为当前需要补偿的旋转偏移。
  return measuredAngle - referenceAngle;
}
