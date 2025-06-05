package api

import (
	"github.com/arm-detector/backend/internal/api/handlers"
	"github.com/arm-detector/backend/internal/api/middleware"
	"github.com/gin-gonic/gin"
)

// Router API路由
type Router struct {
	engine         *gin.Engine
	sensorHandler  *handlers.SensorHandler
	commandHandler *handlers.CommandHandler
}

// NewRouter 创建新的路由器
func NewRouter(sensorHandler *handlers.SensorHandler, commandHandler *handlers.CommandHandler) *Router {
	return &Router{
		engine:         gin.Default(),
		sensorHandler:  sensorHandler,
		commandHandler: commandHandler,
	}
}

// SetupRoutes 设置API路由
func (r *Router) SetupRoutes() {
	r.engine.Use(middleware.Logger())
	
	api := r.engine.Group("/api/v1")
	{
		// 传感器数据API
		api.GET("/sensor", r.sensorHandler.GetSensorDataList)
		api.GET("/sensor/:id", r.sensorHandler.GetSensorData)
		
		// 设备命令API
		api.POST("/device/:device_id/command", r.commandHandler.SendCommand)
		api.GET("/command/:request_id", r.commandHandler.GetCommandStatus)
	}
}

// Run 运行HTTP服务器
func (r *Router) Run(addr string) error {
	return r.engine.Run(addr)
} 