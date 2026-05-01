SET @stmt = IF(
    (SELECT COUNT(*)
     FROM information_schema.columns
     WHERE table_schema = DATABASE() AND table_name = 'point_records' AND column_name = 'laser_content') = 0,
    'ALTER TABLE point_records ADD COLUMN laser_content VARCHAR(255) NOT NULL DEFAULT '''' AFTER read_grade',
    'SELECT 1'
);
PREPARE add_laser_content_stmt FROM @stmt;
EXECUTE add_laser_content_stmt;
DEALLOCATE PREPARE add_laser_content_stmt;

SET @stmt = IF(
    (SELECT COUNT(*)
     FROM information_schema.statistics
     WHERE table_schema = DATABASE() AND table_name = 'point_records' AND index_name = 'idx_point_laser_content') = 0,
    'ALTER TABLE point_records ADD KEY idx_point_laser_content (laser_content(191))',
    'SELECT 1'
);
PREPARE add_laser_content_index_stmt FROM @stmt;
EXECUTE add_laser_content_index_stmt;
DEALLOCATE PREPARE add_laser_content_index_stmt;

UPDATE point_records
SET laser_content = CONCAT(point_name, '-LASER')
WHERE laser_content = '';

SET @stmt = IF(
    (SELECT column_default
     FROM information_schema.columns
     WHERE table_schema = DATABASE() AND table_name = 'point_records' AND column_name = 'is_laser') <> '0',
    'ALTER TABLE point_records MODIFY COLUMN is_laser TINYINT(1) NOT NULL DEFAULT 0',
    'SELECT 1'
);
PREPARE normalize_is_laser_stmt FROM @stmt;
EXECUTE normalize_is_laser_stmt;
DEALLOCATE PREPARE normalize_is_laser_stmt;
