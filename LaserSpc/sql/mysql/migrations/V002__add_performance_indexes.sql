SET @stmt = IF(
    (SELECT COUNT(*) FROM information_schema.statistics
     WHERE table_schema = DATABASE() AND table_name = 'board_records' AND index_name = 'idx_board_program_name') = 0,
    'ALTER TABLE board_records ADD INDEX idx_board_program_name (program_name)',
    'SELECT 1'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @stmt = IF(
    (SELECT COUNT(*) FROM information_schema.statistics
     WHERE table_schema = DATABASE() AND table_name = 'board_records' AND index_name = 'idx_board_device_name') = 0,
    'ALTER TABLE board_records ADD INDEX idx_board_device_name (device_name)',
    'SELECT 1'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @stmt = IF(
    (SELECT COUNT(*) FROM information_schema.statistics
     WHERE table_schema = DATABASE() AND table_name = 'board_records' AND index_name = 'idx_board_line_event') = 0,
    'ALTER TABLE board_records ADD INDEX idx_board_line_event (line_name, event_time)',
    'SELECT 1'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @stmt = IF(
    (SELECT COUNT(*) FROM information_schema.statistics
     WHERE table_schema = DATABASE() AND table_name = 'board_records' AND index_name = 'idx_board_result_line_event') = 0,
    'ALTER TABLE board_records ADD INDEX idx_board_result_line_event (result, line_name, event_time)',
    'SELECT 1'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @stmt = IF(
    (SELECT COUNT(*) FROM information_schema.statistics
     WHERE table_schema = DATABASE() AND table_name = 'point_records' AND index_name = 'idx_point_line_end_time') = 0,
    'ALTER TABLE point_records ADD INDEX idx_point_line_end_time (line_name, end_time)',
    'SELECT 1'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @stmt = IF(
    (SELECT COUNT(*) FROM information_schema.statistics
     WHERE table_schema = DATABASE() AND table_name = 'point_records' AND index_name = 'idx_point_result_line_end_time') = 0,
    'ALTER TABLE point_records ADD INDEX idx_point_result_line_end_time (result, line_name, end_time)',
    'SELECT 1'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
