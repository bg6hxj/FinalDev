-- 检查数据库是否存在，如果不存在则创建
SELECT 'CREATE DATABASE arm_detector_db'
WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = 'arm_detector_db'); 