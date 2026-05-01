CREATE TABLE IF NOT EXISTS board_records (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    board_code VARCHAR(64) NOT NULL,
    result VARCHAR(8) NOT NULL,
    line_name VARCHAR(32) NOT NULL,
    program_name VARCHAR(64) NOT NULL,
    device_name VARCHAR(64) NOT NULL,
    operator_name VARCHAR(64) NOT NULL,
    event_time DATETIME NOT NULL,
    UNIQUE KEY uk_board_code (board_code),
    KEY idx_board_event_time (event_time),
    KEY idx_board_program_name (program_name),
    KEY idx_board_device_name (device_name),
    KEY idx_board_line_program_device (line_name, program_name, device_name),
    KEY idx_board_line_event (line_name, event_time),
    KEY idx_board_result (result),
    KEY idx_board_result_line_event (result, line_name, event_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

CREATE TABLE IF NOT EXISTS point_records (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    board_code VARCHAR(64) NOT NULL,
    point_name VARCHAR(64) NOT NULL,
    result VARCHAR(8) NOT NULL,
    read_grade VARCHAR(16) NOT NULL,
    laser_content VARCHAR(255) NOT NULL DEFAULT '',
    read_code_content VARCHAR(255) NOT NULL DEFAULT '',
    is_laser TINYINT(1) NOT NULL DEFAULT 0,
    is_read_code TINYINT(1) NOT NULL DEFAULT 0,
    line_name VARCHAR(32) NOT NULL,
    program_name VARCHAR(64) NOT NULL,
    device_name VARCHAR(64) NOT NULL,
    start_time DATETIME NOT NULL,
    end_time DATETIME NOT NULL,
    detail_json_path VARCHAR(512) NOT NULL DEFAULT '',
    UNIQUE KEY uk_point_board_code_name (board_code, point_name),
    KEY idx_point_board_code (board_code),
    KEY idx_point_end_time (end_time),
    KEY idx_point_line_program_device (line_name, program_name, device_name),
    KEY idx_point_line_end_time (line_name, end_time),
    KEY idx_point_result_grade (result, read_grade),
    KEY idx_point_laser_content (laser_content(191)),
    KEY idx_point_read_code_content (read_code_content(191)),
    KEY idx_point_result_line_end_time (result, line_name, end_time),
    CONSTRAINT fk_point_board_code
        FOREIGN KEY (board_code) REFERENCES board_records(board_code)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
