DELETE pr1
FROM point_records pr1
INNER JOIN point_records pr2
    ON pr1.board_code = pr2.board_code
   AND pr1.point_name = pr2.point_name
   AND pr1.id < pr2.id;

SET @stmt = (
    SELECT IF(
        EXISTS(
            SELECT 1
            FROM information_schema.statistics
            WHERE table_schema = DATABASE()
              AND table_name = 'point_records'
              AND index_name = 'uk_point_board_code_name'
        ),
        'SELECT 1',
        'ALTER TABLE point_records ADD CONSTRAINT uk_point_board_code_name UNIQUE (board_code, point_name)'
    )
);
PREPARE add_unique_point_key FROM @stmt;
EXECUTE add_unique_point_key;
DEALLOCATE PREPARE add_unique_point_key;
