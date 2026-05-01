SET @stmt = IF(
    (SELECT COUNT(*)
     FROM information_schema.columns
     WHERE table_schema = DATABASE() AND table_name = 'point_records' AND column_name = 'is_laser') = 0,
    'ALTER TABLE point_records ADD COLUMN is_laser TINYINT(1) NOT NULL DEFAULT 1 AFTER read_code_content',
    'SELECT 1'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @stmt = IF(
    (SELECT COUNT(*)
     FROM information_schema.columns
     WHERE table_schema = DATABASE() AND table_name = 'point_records' AND column_name = 'is_read_code') = 0,
    'ALTER TABLE point_records ADD COLUMN is_read_code TINYINT(1) NOT NULL DEFAULT 0 AFTER is_laser',
    'SELECT 1'
);
PREPARE stmt FROM @stmt;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

UPDATE point_records
SET is_read_code = 1
WHERE TRIM(COALESCE(read_code_content, '')) <> ''
  AND COALESCE(is_read_code, 0) = 0;
