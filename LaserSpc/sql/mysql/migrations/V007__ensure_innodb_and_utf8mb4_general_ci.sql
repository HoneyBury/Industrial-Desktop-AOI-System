ALTER DATABASE CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;

SET FOREIGN_KEY_CHECKS=0;

ALTER TABLE point_records DROP FOREIGN KEY fk_point_board_code;

ALTER TABLE board_records ENGINE=InnoDB;
ALTER TABLE board_records CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;

ALTER TABLE point_records ENGINE=InnoDB;
ALTER TABLE point_records CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
ALTER TABLE point_records
    ADD CONSTRAINT fk_point_board_code
    FOREIGN KEY (board_code) REFERENCES board_records(board_code)
    ON DELETE CASCADE;

ALTER TABLE point_records
    DROP INDEX idx_point_read_code_content,
    ADD INDEX idx_point_read_code_content (read_code_content(191));

SET FOREIGN_KEY_CHECKS=1;
