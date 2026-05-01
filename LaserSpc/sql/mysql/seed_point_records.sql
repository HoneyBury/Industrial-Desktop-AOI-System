INSERT INTO point_records (
    board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code,
    line_name, program_name, device_name, start_time, end_time, detail_json_path
)
SELECT
    'BD-240301-0001', 'Code-A1', 'OK', 'A', 'Code-A1-LASER', 'READ-A1', 1, 1,
    'L1', 'Program-A', 'Laser-01', '2026-03-10 08:15:50', '2026-03-10 08:16:40', 'seed_data/point_details/BD-240301-0001_Code-A1.json'
FROM DUAL
WHERE EXISTS (SELECT 1 FROM board_records WHERE board_code = 'BD-240301-0001')
  AND NOT EXISTS (SELECT 1 FROM point_records WHERE board_code = 'BD-240301-0001' AND point_name = 'Code-A1');

INSERT INTO point_records (
    board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code,
    line_name, program_name, device_name, start_time, end_time, detail_json_path
)
SELECT
    'BD-240301-0002', 'Code-A2', 'OK', 'A', 'Code-A2-LASER', 'READ-A2', 1, 1,
    'L1', 'Program-A', 'Laser-01', '2026-03-10 08:17:20', '2026-03-10 08:18:20', 'seed_data/point_details/BD-240301-0002_Code-A2.json'
FROM DUAL
WHERE EXISTS (SELECT 1 FROM board_records WHERE board_code = 'BD-240301-0002')
  AND NOT EXISTS (SELECT 1 FROM point_records WHERE board_code = 'BD-240301-0002' AND point_name = 'Code-A2');

INSERT INTO point_records (
    board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code,
    line_name, program_name, device_name, start_time, end_time, detail_json_path
)
SELECT
    'BD-240301-0003', 'MarkOffset', 'NG', 'C', 'MarkOffset-LASER', 'READ-B1', 1, 1,
    'L1', 'Program-B', 'Laser-02', '2026-03-10 08:17:50', '2026-03-10 08:18:40', 'seed_data/point_details/BD-240301-0003_MarkOffset.json'
FROM DUAL
WHERE EXISTS (SELECT 1 FROM board_records WHERE board_code = 'BD-240301-0003')
  AND NOT EXISTS (SELECT 1 FROM point_records WHERE board_code = 'BD-240301-0003' AND point_name = 'MarkOffset');

INSERT INTO point_records (
    board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code,
    line_name, program_name, device_name, start_time, end_time, detail_json_path
)
SELECT
    'BD-240301-0004', 'Code-C1', 'OK', 'B', 'Code-C1-LASER', 'READ-C1', 1, 1,
    'L2', 'Program-C', 'Laser-03', '2026-03-10 08:18:20', '2026-03-10 08:19:10', 'seed_data/point_details/BD-240301-0004_Code-C1.json'
FROM DUAL
WHERE EXISTS (SELECT 1 FROM board_records WHERE board_code = 'BD-240301-0004')
  AND NOT EXISTS (SELECT 1 FROM point_records WHERE board_code = 'BD-240301-0004' AND point_name = 'Code-C1');

INSERT INTO point_records (
    board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code,
    line_name, program_name, device_name, start_time, end_time, detail_json_path
)
SELECT
    'BD-240301-0005', 'CodeBlur', 'NG', 'D', 'CodeBlur-LASER', 'READ-D1', 1, 1,
    'L2', 'Program-D', 'Laser-04', '2026-03-10 08:18:50', '2026-03-10 08:19:40', 'seed_data/point_details/BD-240301-0005_CodeBlur.json'
FROM DUAL
WHERE EXISTS (SELECT 1 FROM board_records WHERE board_code = 'BD-240301-0005')
  AND NOT EXISTS (SELECT 1 FROM point_records WHERE board_code = 'BD-240301-0005' AND point_name = 'CodeBlur');

INSERT INTO point_records (
    board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code,
    line_name, program_name, device_name, start_time, end_time, detail_json_path
)
SELECT
    'BD-240301-0006', 'Code-E1', 'OK', 'A', 'Code-E1-LASER', 'READ-E1', 1, 1,
    'L3', 'Program-E', 'Laser-05', '2026-03-10 08:19:10', '2026-03-10 08:20:00', 'seed_data/point_details/BD-240301-0006_Code-E1.json'
FROM DUAL
WHERE EXISTS (SELECT 1 FROM board_records WHERE board_code = 'BD-240301-0006')
  AND NOT EXISTS (SELECT 1 FROM point_records WHERE board_code = 'BD-240301-0006' AND point_name = 'Code-E1');

INSERT INTO point_records (
    board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code,
    line_name, program_name, device_name, start_time, end_time, detail_json_path
)
SELECT
    'BD-240301-0007', 'Code-F1', 'OK', 'B', 'Code-F1-LASER', 'READ-F1', 1, 1,
    'L3', 'Program-F', 'Laser-06', '2026-03-10 08:19:20', '2026-03-10 08:20:10', 'seed_data/point_details/BD-240301-0007_Code-F1.json'
FROM DUAL
WHERE EXISTS (SELECT 1 FROM board_records WHERE board_code = 'BD-240301-0007')
  AND NOT EXISTS (SELECT 1 FROM point_records WHERE board_code = 'BD-240301-0007' AND point_name = 'Code-F1');

INSERT INTO point_records (
    board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code,
    line_name, program_name, device_name, start_time, end_time, detail_json_path
)
SELECT
    'BD-240301-0008', 'ContrastLow', 'NG', 'C', 'ContrastLow-LASER', 'READ-B2', 1, 1,
    'L1', 'Program-B', 'Laser-02', '2026-03-10 08:20:00', '2026-03-10 08:20:50', 'seed_data/point_details/BD-240301-0008_ContrastLow.json'
FROM DUAL
WHERE EXISTS (SELECT 1 FROM board_records WHERE board_code = 'BD-240301-0008')
  AND NOT EXISTS (SELECT 1 FROM point_records WHERE board_code = 'BD-240301-0008' AND point_name = 'ContrastLow');

INSERT INTO point_records (
    board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code,
    line_name, program_name, device_name, start_time, end_time, detail_json_path
)
SELECT
    'BD-240301-0009', 'Code-C2', 'OK', 'A', 'Code-C2-LASER', 'READ-C2', 1, 1,
    'L2', 'Program-C', 'Laser-03', '2026-03-10 08:20:40', '2026-03-10 08:21:30', 'seed_data/point_details/BD-240301-0009_Code-C2.json'
FROM DUAL
WHERE EXISTS (SELECT 1 FROM board_records WHERE board_code = 'BD-240301-0009')
  AND NOT EXISTS (SELECT 1 FROM point_records WHERE board_code = 'BD-240301-0009' AND point_name = 'Code-C2');

INSERT INTO point_records (
    board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code,
    line_name, program_name, device_name, start_time, end_time, detail_json_path
)
SELECT
    'BD-240301-0010', 'PrintShift', 'NG', 'D', 'PrintShift-LASER', 'READ-E2', 1, 1,
    'L3', 'Program-E', 'Laser-05', '2026-03-10 08:21:10', '2026-03-10 08:22:00', 'seed_data/point_details/BD-240301-0010_PrintShift.json'
FROM DUAL
WHERE EXISTS (SELECT 1 FROM board_records WHERE board_code = 'BD-240301-0010')
  AND NOT EXISTS (SELECT 1 FROM point_records WHERE board_code = 'BD-240301-0010' AND point_name = 'PrintShift');
