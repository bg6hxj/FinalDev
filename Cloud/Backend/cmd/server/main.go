package main

import (
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"

	"github.com/arm-detector/backend/internal/api"
	"github.com/arm-detector/backend/internal/api/handlers"
	"github.com/arm-detector/backend/internal/config"
	"github.com/arm-detector/backend/internal/mqtt"
	"github.com/arm-detector/backend/internal/service"
	"github.com/arm-detector/backend/internal/storage"
)

func main() {
	// 加载配置
	cfg, err := config.LoadConfig()
	if err != nil {
		log.Fatalf("加载配置失败: %v", err)
	}
	
	// 初始化数据库连接
	db, err := storage.NewDatabase(&cfg.Database)
	if err != nil {
		log.Fatalf("初始化数据库失败: %v", err)
	}
	defer db.Close()
	
	// 创建数据库仓库
	repo := storage.NewRepository(db)
	
	// 初始化MQTT客户端
	log.Printf("Configured MQTT Broker URL: %s", cfg.MQTT.BrokerURL)
	mqttClient, err := mqtt.NewClient(&cfg.MQTT)
	if err != nil {
		log.Fatalf("初始化MQTT客户端失败: %v", err)
	}
	defer mqttClient.Disconnect()
	
	// 创建MQTT命令发布器
	commandPublisher := mqtt.NewCommandPublisher(mqttClient, cfg.MQTT.TopicCommandPublishTemplate)
	
	// 创建MQTT传感器数据处理器
	sensorDataHandler := mqtt.NewSensorDataHandler(
		mqttClient,
		repo,
		cfg.MQTT.TopicSensors,
		cfg.MQTT.TopicResponseSubscribePattern,
	)
	
	// 启动MQTT订阅
	if err := sensorDataHandler.Start(); err != nil {
		log.Fatalf("启动MQTT订阅失败: %v", err)
	}
	
	// 创建服务
	sensorService := service.NewSensorService(repo)
	commandService := service.NewCommandService(repo, commandPublisher)
	
	// 创建HTTP处理器
	sensorHandler := handlers.NewSensorHandler(sensorService)
	commandHandler := handlers.NewCommandHandler(commandService)
	
	// 创建路由
	router := api.NewRouter(sensorHandler, commandHandler)
	router.SetupRoutes()
	
	// 启动HTTP服务器
	serverAddr := fmt.Sprintf("%s:%d", cfg.Server.Host, cfg.Server.Port)
	log.Printf("HTTP服务器启动，监听: %s", serverAddr)
	
	// 监听中断信号
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, os.Interrupt, syscall.SIGTERM)
	
	// 在单独的goroutine中启动服务器，以便可以优雅地关闭
	go func() {
		if err := router.Run(serverAddr); err != nil {
			log.Fatalf("启动HTTP服务器失败: %v", err)
		}
	}()
	
	// 等待中断信号
	<-sigCh
	log.Println("正在关闭服务器...")
} 