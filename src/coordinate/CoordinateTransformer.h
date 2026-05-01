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

// 6-layer coordinate chain used by the AOI system:
//
//   CameraPixel ──(undistort)──▶ UndistortedPixel ──(×pixelScale)──▶ ImagePhysicalMm
//       ──(rigid)──▶ Product ──(rigid+origin)──▶ Machine ──(offset)──▶ Laser
//
// Distortion uses a 2-parameter radial model (k₁, k₂) relative to the
// optical center.  Each rigid transform applies rotation first then
// translation.
struct CoordinateTransformChain {
  PixelPoint opticalCenterPixel;
  std::vector<double> distortionCoefficients; // [k1, k2, ...]
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

  // ── 工业坐标链新增方法（docs/industrial_calibration_and_coordinate_plan.md §5.2） ──

  /// 机械坐标 → 产品坐标（productToMechanical 的逆变换）
  [[nodiscard]] MillimeterPoint machineToProduct(const MechanicalPose &machinePose,
                                                  const MechanicalPose &originPose) const;

  /// 对产品点应用 Mark 刚体变换
  [[nodiscard]] static MillimeterPoint applyMarkTransform(const MillimeterPoint &productPoint,
                                                          const RigidTransform2D &markTransform);

  /// 对机械坐标应用激光偏移
  [[nodiscard]] MechanicalPose applyLaserOffset(const MechanicalPose &machinePose,
                                                 const MillimeterPoint &laserOffsetMm) const;

  /// 计算像素点击 → 相机移动量 (mm)
  [[nodiscard]] MillimeterPoint imageClickToMoveDelta(const PixelPoint &clickPixel,
                                                       const PixelPoint &imageCenter) const;

  /// 产品点 → 相机应当移动到的机械目标位姿
  [[nodiscard]] MechanicalPose targetProductPointToCameraPosition(
      const MillimeterPoint &productPoint, const MechanicalPose &originPose) const;

  /// 产品点 → 激光应当移动到的机械目标位姿（含偏移补偿）
  [[nodiscard]] MechanicalPose targetProductPointToLaserPosition(
      const MillimeterPoint &productPoint, const MechanicalPose &originPose,
      const MillimeterPoint &laserOffsetMm) const;

  [[nodiscard]] static MillimeterPoint applyRigidTransform(const MillimeterPoint &point,
                                                           const RigidTransform2D &transform);

private:
  CoordinateTransformChain chain_ {};
};
