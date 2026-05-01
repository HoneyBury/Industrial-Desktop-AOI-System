# Mark 对位设计

Mark 对位用于补偿"标准检测程序坐标"与"当前产品实际姿态"之间的平移与旋转差异。

## 对位模式

`MarkAlignmentSolver` 根据参考 Mark 和实测 Mark 的数量自动选择模式：

| 模式 | Mark 数量 | 算法 | 输出 |
|------|----------|------|------|
| `SingleMarkTranslation` | 1 | 直接像素偏移 | 平移 |
| `DualMarkRigid` | 2 | 向量角度差 + 平移 | 平移 + 旋转 + 残差 |
| `MultiMarkLeastSquares` | 3+ | Kabsch-Umeyama 2D 闭式 | 平移 + 旋转 + RMS 残差 |

## MultiMarkLeastSquares 算法

1. 计算参考 Mark 和实测 Mark 的质心
2. 构建 2×2 互协方差矩阵 H
3. 通过 Kabsch 2D 闭式 `θ = atan2(h01-h10, h00+h11)` 求解最优旋转
4. 平移 = 实测质心 − R × 参考质心
5. 计算所有 Mark 点的 RMS 像素残差

## 数据流

```
referenceMarks (ProgramModel) + measuredMarks (MarkDetector)
  → MarkAlignmentSolver.solve()
  → MarkAlignmentResult { pixelOffset, millimeterOffset, rotationDegrees, residualRmsPx, productCompensation }
  → PreLaserStep 应用补偿 + 激光偏移 → 运动平台执行
```

## 当前测试覆盖

- 单 Mark 平移精度验证
- 双 Mark 刚体（平移+旋转）精度验证
- 四 Mark 最小二乘拟合（已知刚体变换恢复精度验证）
