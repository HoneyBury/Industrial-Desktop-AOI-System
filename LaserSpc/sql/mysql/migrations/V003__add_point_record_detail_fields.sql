SET @stmt = IF(
    (SELECT COUNT(*) FROM information_schema.columns
     WHERE table_schema = DATABASE() AND table_name = 'point_records' AND column_name = 'read_code_content') = 0,
    'ALTER TABLE point_records ADD COLUMN read_code_content VARCHAR(255) NOT NULL DEFAULT '''' AFTER read_grade',
    'SELECT 1'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @stmt = IF(
    (SELECT COUNT(*) FROM information_schema.columns
     WHERE table_schema = DATABASE() AND table_name = 'point_records' AND column_name = 'detail_json_path') = 0,
    'ALTER TABLE point_records ADD COLUMN detail_json_path VARCHAR(512) NOT NULL DEFAULT '''' AFTER end_time',
    'SELECT 1'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @stmt = IF(
    (SELECT COUNT(*) FROM information_schema.statistics
     WHERE table_schema = DATABASE() AND table_name = 'point_records' AND index_name = 'idx_point_read_code_content') = 0,
    'ALTER TABLE point_records ADD INDEX idx_point_read_code_content (read_code_content(191))',
    'SELECT 1'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
