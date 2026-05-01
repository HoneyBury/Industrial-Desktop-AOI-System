#pragma once

/// 工业设备轴类型
enum class MotionAxis {
  Conveyor,  // 传送带
  Stopper,   // 挡板 / 定位机构
  CameraX,   // 相机 X 轴
  CameraY,   // 相机 Y 轴
  LaserX,    // 激光 X 轴（独立于相机的激光运动轴）
  LaserY,    // 激光 Y 轴
  Z,         // Z 轴（预留，高度）
  R,         // 旋转轴（预留）
};

/// 轴运动状态
enum class AxisState {
  Idle,
  Moving,
  Homing,
  Done,
  Alarm,
  EmergencyStopped,
};
