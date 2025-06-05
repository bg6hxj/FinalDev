-- 创建传感器数据表
CREATE TABLE IF NOT EXISTS sensor_data (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(50) NOT NULL,
    timestamp TIMESTAMPTZ NOT NULL,
    temperature DECIMAL(5, 2) NOT NULL,
    humidity DECIMAL(5, 2) NOT NULL,
    co_ppm DECIMAL(6, 2) NOT NULL,
    dust_density DECIMAL(6, 2) NOT NULL,
    alarm_status VARCHAR(50) NOT NULL,
    received_at TIMESTAMPTZ NOT NULL
);

-- 创建命令日志表
CREATE TABLE IF NOT EXISTS command_logs (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(50) NOT NULL,
    command VARCHAR(50) NOT NULL,
    request_id VARCHAR(50) NOT NULL UNIQUE,
    status VARCHAR(20) NOT NULL,
    sent_at TIMESTAMP NOT NULL,
    response TEXT
);

-- 创建传感器数据索引
CREATE INDEX IF NOT EXISTS idx_sensor_data_device_id ON sensor_data(device_id);
CREATE INDEX IF NOT EXISTS idx_sensor_data_timestamp ON sensor_data(timestamp);
CREATE INDEX IF NOT EXISTS idx_sensor_data_device_timestamp ON sensor_data(device_id, timestamp);

-- 创建命令日志索引
CREATE INDEX IF NOT EXISTS idx_command_logs_device_id ON command_logs(device_id);
CREATE INDEX IF NOT EXISTS idx_command_logs_request_id ON command_logs(request_id); 