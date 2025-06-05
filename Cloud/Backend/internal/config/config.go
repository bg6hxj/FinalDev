package config

import (
	"fmt"
	"log"
	"os"
	"strings"

	"github.com/spf13/viper"
)

// Config 保存所有应用程序配置
type Config struct {
	Server   ServerConfig
	Database DatabaseConfig
	MQTT     MQTTConfig
	Logging  LoggingConfig
}

// ServerConfig 服务器配置
type ServerConfig struct {
	Port int
	Host string
}

// DatabaseConfig 数据库配置
type DatabaseConfig struct {
	Host     string
	Port     int
	User     string
	Password string
	DBName   string
	SSLMode  string
}

// MQTTConfig MQTT配置
type MQTTConfig struct {
	BrokerURL                  string `mapstructure:"broker_url"`
	ClientID                   string `mapstructure:"client_id"`
	TopicSensors               string `mapstructure:"topic_sensors"`
	TopicCommandPublishTemplate string `mapstructure:"topic_command_publish_template"`
	TopicResponseSubscribePattern string `mapstructure:"topic_response_subscribe_pattern"`
	Username                   string `mapstructure:"username"`
	Password                   string `mapstructure:"password"`
}

// LoggingConfig 日志配置
type LoggingConfig struct {
	Level string
}

// GetDSN 获取数据库连接字符串
func (c *DatabaseConfig) GetDSN() string {
	return fmt.Sprintf("host=%s port=%d user=%s password=%s dbname=%s sslmode=%s TimeZone=UTC",
		c.Host, c.Port, c.User, c.Password, c.DBName, c.SSLMode)
}

// LoadConfig 从文件和环境变量加载配置
func LoadConfig() (*Config, error) {
	// 设置默认值
	viper.SetDefault("server.port", 8080)
	viper.SetDefault("server.host", "0.0.0.0")
	viper.SetDefault("database.host", "db")
	viper.SetDefault("database.port", 5432)
	viper.SetDefault("database.user", "postgres")
	viper.SetDefault("database.password", "postgres")
	viper.SetDefault("database.dbname", "arm_detector_db")
	viper.SetDefault("database.sslmode", "disable")
	viper.SetDefault("mqtt.broker_url", "tcp://mqtt_broker:1883")
	viper.SetDefault("mqtt.client_id", "arm-detector-backend")
	viper.SetDefault("mqtt.topic_sensors", "armdetector/sensor/data")
	viper.SetDefault("mqtt.topic_command_publish_template", "armdetector/device/%s/command")
	viper.SetDefault("mqtt.topic_response_subscribe_pattern", "armdetector/device/+/response")
	viper.SetDefault("mqtt.username", "")
	viper.SetDefault("mqtt.password", "")
	viper.SetDefault("logging.level", "info")

	// 设置配置文件路径
	viper.SetConfigName("config")
	viper.SetConfigType("yaml")
	viper.AddConfigPath("./configs")
	viper.AddConfigPath(".")

	// 尝试读取配置文件
	if err := viper.ReadInConfig(); err != nil {
		if _, ok := err.(viper.ConfigFileNotFoundError); !ok {
			// 如果配置文件存在但读取错误，返回错误
			return nil, fmt.Errorf("读取配置文件错误: %w", err)
		}
		// 配置文件不存在，只使用环境变量和默认值
		log.Println("未找到配置文件，使用环境变量和默认值")
	}

	// 允许从环境变量读取配置
	viper.SetEnvKeyReplacer(strings.NewReplacer(".", "_"))
	viper.AutomaticEnv()

	// 从环境变量中读取覆盖
	// SERVER_PORT、DATABASE_HOST 等环境变量将被优先采用
	envMap := map[string]string{
		"SERVER_PORT":        "server.port",
		"SERVER_HOST":        "server.host",
		"DATABASE_HOST":      "database.host",
		"DATABASE_PORT":      "database.port",
		"DATABASE_USER":      "database.user",
		"DATABASE_PASSWORD":  "database.password",
		"DATABASE_DBNAME":    "database.dbname",
		"DATABASE_SSLMODE":   "database.sslmode",
		"MQTT_BROKER_URL":    "mqtt.broker_url",
		"MQTT_CLIENT_ID":     "mqtt.client_id",
		"MQTT_TOPIC_SENSORS": "mqtt.topic_sensors",
		"MQTT_TOPIC_COMMAND_PUBLISH_TEMPLATE": "mqtt.topic_command_publish_template",
		"MQTT_TOPIC_RESPONSE_SUBSCRIBE_PATTERN": "mqtt.topic_response_subscribe_pattern",
		"MQTT_USERNAME":      "mqtt.username",
		"MQTT_PASSWORD":      "mqtt.password",
		"LOGGING_LEVEL":      "logging.level",
	}

	for env, path := range envMap {
		if val := os.Getenv(env); val != "" {
			viper.Set(path, val)
		}
	}

	// 将配置映射到结构体
	var config Config
	log.Printf("Viper MQTT_BROKER_URL: %s", viper.GetString("mqtt.broker_url")) // 添加调试日志
	if err := viper.Unmarshal(&config); err != nil {
		return nil, fmt.Errorf("无法解析配置: %w", err)
	}

	return &config, nil
} 