package storage

import (
	"fmt"
	"log"
	"time"

	"github.com/arm-detector/backend/internal/config"
	"github.com/jmoiron/sqlx"
	_ "github.com/lib/pq"
)

// Database 表示数据库连接
type Database struct {
	DB *sqlx.DB
}

// NewDatabase 创建并初始化数据库连接
func NewDatabase(cfg *config.DatabaseConfig) (*Database, error) {
	dsn := cfg.GetDSN()
	
	// 添加重试逻辑
	var db *sqlx.DB
	var err error
	maxRetries := 10
	retryInterval := 3 * time.Second
	
	log.Printf("正在连接数据库: %s:%d/%s", cfg.Host, cfg.Port, cfg.DBName)
	
	for i := 0; i < maxRetries; i++ {
		db, err = sqlx.Connect("postgres", dsn)
		if err == nil {
			break
		}
		
		log.Printf("数据库连接尝试 %d/%d 失败: %v, 将在 %v 后重试", i+1, maxRetries, err, retryInterval)
		
		// 检查错误信息，如果是"database does not exist"，尝试创建数据库
		if i > 2 && err.Error() == fmt.Sprintf(`pq: database "%s" does not exist`, cfg.DBName) {
			log.Printf("尝试创建数据库: %s", cfg.DBName)
			if err := createDatabase(cfg); err != nil {
				log.Printf("创建数据库失败: %v", err)
			} else {
				log.Printf("数据库创建成功，继续尝试连接")
			}
		}
		
		time.Sleep(retryInterval)
	}
	
	if err != nil {
		return nil, fmt.Errorf("连接数据库失败，已尝试 %d 次: %w", maxRetries, err)
	}
	
	// 测试连接
	if err := db.Ping(); err != nil {
		return nil, fmt.Errorf("数据库Ping失败: %w", err)
	}
	
	log.Println("数据库连接成功")
	return &Database{DB: db}, nil
}

// 创建数据库
func createDatabase(cfg *config.DatabaseConfig) error {
	// 连接到默认的 postgres 数据库
	defaultDSN := fmt.Sprintf("host=%s port=%d user=%s password=%s dbname=postgres sslmode=%s",
		cfg.Host, cfg.Port, cfg.User, cfg.Password, cfg.SSLMode)
	
	db, err := sqlx.Connect("postgres", defaultDSN)
	if err != nil {
		return fmt.Errorf("连接默认数据库失败: %w", err)
	}
	defer db.Close()
	
	// 创建数据库
	query := fmt.Sprintf("CREATE DATABASE %s", cfg.DBName)
	_, err = db.Exec(query)
	if err != nil {
		return fmt.Errorf("执行创建数据库SQL失败: %w", err)
	}
	
	return nil
}

// Close 关闭数据库连接
func (d *Database) Close() error {
	if d.DB != nil {
		return d.DB.Close()
	}
	return nil
} 