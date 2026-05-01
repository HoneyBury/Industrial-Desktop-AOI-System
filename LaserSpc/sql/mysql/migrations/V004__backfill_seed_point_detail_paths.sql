UPDATE point_records
SET detail_json_path = CASE
    WHEN board_code = 'BD-240301-0001' AND point_name = 'Code-A1' THEN 'seed_data/point_details/BD-240301-0001_Code-A1.json'
    WHEN board_code = 'BD-240301-0002' AND point_name = 'Code-A2' THEN 'seed_data/point_details/BD-240301-0002_Code-A2.json'
    WHEN board_code = 'BD-240301-0003' AND point_name = 'MarkOffset' THEN 'seed_data/point_details/BD-240301-0003_MarkOffset.json'
    WHEN board_code = 'BD-240301-0004' AND point_name = 'Code-C1' THEN 'seed_data/point_details/BD-240301-0004_Code-C1.json'
    WHEN board_code = 'BD-240301-0005' AND point_name = 'CodeBlur' THEN 'seed_data/point_details/BD-240301-0005_CodeBlur.json'
    WHEN board_code = 'BD-240301-0006' AND point_name = 'Code-E1' THEN 'seed_data/point_details/BD-240301-0006_Code-E1.json'
    WHEN board_code = 'BD-240301-0007' AND point_name = 'Code-F1' THEN 'seed_data/point_details/BD-240301-0007_Code-F1.json'
    WHEN board_code = 'BD-240301-0008' AND point_name = 'ContrastLow' THEN 'seed_data/point_details/BD-240301-0008_ContrastLow.json'
    WHEN board_code = 'BD-240301-0009' AND point_name = 'Code-C2' THEN 'seed_data/point_details/BD-240301-0009_Code-C2.json'
    WHEN board_code = 'BD-240301-0010' AND point_name = 'PrintShift' THEN 'seed_data/point_details/BD-240301-0010_PrintShift.json'
    ELSE detail_json_path
END
WHERE TRIM(COALESCE(detail_json_path, '')) = '';
