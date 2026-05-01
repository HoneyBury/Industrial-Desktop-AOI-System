# 数据库设计

DatabaseManager 已从占位桩升级为真实 SQLite3 实现 (`AOI_HAS_SQLITE`)，无 SQLite3 时自动回退到桩模式。

## Schema

### `inspection_results` — 检测结果

| 列 | 类型 | 说明 |
|-----|------|------|
| `id` | INTEGER PK | 自增主键 |
| `board_id` | TEXT NOT NULL | 板号 |
| `timestamp` | TEXT NOT NULL | ISO 8601 时间戳 |
| `program_name` | TEXT | 检测程序名 |
| `image_path` | TEXT | 采集图像路径 |
| `ai_label` | TEXT | AI 判定标签 (ok/ng) |
| `ai_confidence` | REAL | AI 置信度 |
| `final_decision` | TEXT | 最终判定 (OK/NG) |
| `marks_count` | INTEGER | Mark 点数量 |
| `rois_count` | INTEGER | ROI 数量 |
| `details_json` | TEXT | 扩展详情 JSON |

索引：`board_id`、`timestamp`

### `calibration_history` — 标定历史

| 列 | 类型 | 说明 |
|-----|------|------|
| `id` | INTEGER PK | 自增主键 |
| `timestamp` | TEXT NOT NULL | ISO 8601 时间戳 |
| `calibration_type` | TEXT NOT NULL | 标定类型 (intrinsic/pixel_scale/origin/laser_offset/mark_reference) |
| `fx`, `fy`, `cx`, `cy` | REAL | 相机内参 |
| `pixel_scale_x`, `pixel_scale_y` | REAL | 像素比例 |
| `origin_x`, `origin_y`, `origin_r` | REAL | 原点位姿 |
| `laser_offset_dx`, `laser_offset_dy` | REAL | 激光偏移 |
| `notes` | TEXT | 备注 |

索引：`calibration_type`

### `board_records` — 板追踪

| 列 | 类型 | 说明 |
|-----|------|------|
| `id` | INTEGER PK | 自增主键 |
| `board_id` | TEXT NOT NULL UNIQUE | 板号 (唯一) |
| `program_name` | TEXT | 检测程序名 |
| `status` | TEXT | 状态 (pending/processing/ok/ng) |
| `created_at` | TEXT | 创建时间 |
| `updated_at` | TEXT | 更新时间 |

## API

| 方法 | 说明 |
|------|------|
| `open(path)` | 打开/创建数据库，自动建表，启用 WAL 模式 |
| `close()` | 关闭数据库 |
| `insertInspectionResult(record)` | 写入检测结果 |
| `queryInspectionResults(limit)` | 按时间倒序查询检测结果 |
| `insertCalibrationRecord(record)` | 写入标定记录 |
| `queryCalibrationHistory(limit)` | 按时间倒序查询标定历史 |
| `countBoardResults(decision, [since])` | 按判定统计板数，支持时间过滤 |

## 设计原则

- 配方数据可版本化（通过标定历史表）
- 检测结果可追溯（板号 + 时间戳索引）
- 配置数据与运行结果数据分表存储
- 预留向 MES/SPC 系统扩展导出的可能
